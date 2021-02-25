/**
 * Plinth High-Performance Streaming Relational Engine
 *
 * plinth_client.c - Native C Client implementation for PLN1 wire protocol.
 */

#include "plinth.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

struct plinth_client {
    int sockfd;
    uint32_t next_corr_id;
    char error_buf[256];
};

static void set_error(plinth_client_t *c, const char *msg) {
    if (c) {
        strncpy(c->error_buf, msg, sizeof(c->error_buf) - 1);
        c->error_buf[sizeof(c->error_buf) - 1] = '\0';
    }
}

static uint32_t read_be32(const uint8_t *b) {
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) |
           ((uint32_t)b[2] << 8)  | (uint32_t)b[3];
}

static void write_be32(uint8_t *b, uint32_t val) {
    b[0] = (uint8_t)((val >> 24) & 0xFF);
    b[1] = (uint8_t)((val >> 16) & 0xFF);
    b[2] = (uint8_t)((val >> 8) & 0xFF);
    b[3] = (uint8_t)(val & 0xFF);
}

static void write_be16(uint8_t *b, uint16_t val) {
    b[0] = (uint8_t)((val >> 8) & 0xFF);
    b[1] = (uint8_t)(val & 0xFF);
}

static int send_exact(int fd, const void *buf, size_t len) {
    const uint8_t *p = (const uint8_t *)buf;
    while (len > 0) {
        ssize_t n = send(fd, p, len, 0);
        if (n <= 0) {
            return -1;
        }
        p += n;
        len -= (size_t)n;
    }
    return 0;
}

static int recv_exact(int fd, void *buf, size_t len) {
    uint8_t *p = (uint8_t *)buf;
    while (len > 0) {
        ssize_t n = recv(fd, p, len, 0);
        if (n <= 0) {
            return -1;
        }
        p += n;
        len -= (size_t)n;
    }
    return 0;
}

static int send_frame(plinth_client_t *c, uint8_t opcode, const void *payload, uint32_t payload_len) {
    uint8_t header[PLINTH_HEADER_SIZE];
    write_be32(&header[0], PLINTH_MAGIC);
    header[4] = PLINTH_PROTOCOL_VER;
    header[5] = opcode;
    write_be16(&header[6], 0); /* flags */
    write_be32(&header[8], ++c->next_corr_id);
    write_be32(&header[12], payload_len);

    uint32_t crc = plinth_crc32c(0, header, PLINTH_HEADER_SIZE);
    if (payload && payload_len > 0) {
        crc = plinth_crc32c(crc, payload, payload_len);
    }

    uint8_t crc_bytes[4];
    write_be32(crc_bytes, crc);

    if (send_exact(c->sockfd, header, sizeof(header)) != 0) {
        set_error(c, "Failed to send header");
        return -1;
    }
    if (payload && payload_len > 0) {
        if (send_exact(c->sockfd, payload, payload_len) != 0) {
            set_error(c, "Failed to send payload");
            return -1;
        }
    }
    if (send_exact(c->sockfd, crc_bytes, sizeof(crc_bytes)) != 0) {
        set_error(c, "Failed to send CRC trailer");
        return -1;
    }
    return 0;
}

static int recv_frame(plinth_client_t *c, uint8_t *out_opcode, uint8_t **out_payload, uint32_t *out_len) {
    uint8_t header[PLINTH_HEADER_SIZE];
    if (recv_exact(c->sockfd, header, sizeof(header)) != 0) {
        set_error(c, "Failed to read response header");
        return -1;
    }

    uint32_t magic = read_be32(&header[0]);
    if (magic != PLINTH_MAGIC) {
        set_error(c, "Invalid magic in response frame");
        return -1;
    }

    *out_opcode = header[5];
    uint32_t payload_len = read_be32(&header[12]);
    *out_len = payload_len;

    uint8_t *payload = NULL;
    if (payload_len > 0) {
        payload = (uint8_t *)malloc(payload_len + 1);
        if (!payload) {
            set_error(c, "Out of memory allocating payload");
            return -1;
        }
        if (recv_exact(c->sockfd, payload, payload_len) != 0) {
            free(payload);
            set_error(c, "Failed to read response payload");
            return -1;
        }
        payload[payload_len] = '\0';
    }

    uint8_t crc_bytes[4];
    if (recv_exact(c->sockfd, crc_bytes, sizeof(crc_bytes)) != 0) {
        free(payload);
        set_error(c, "Failed to read response CRC trailer");
        return -1;
    }

    uint32_t expected_crc = read_be32(crc_bytes);
    uint32_t actual_crc = plinth_crc32c(0, header, sizeof(header));
    if (payload && payload_len > 0) {
        actual_crc = plinth_crc32c(actual_crc, payload, payload_len);
    }

    if (actual_crc != expected_crc) {
        free(payload);
        set_error(c, "CRC-32C checksum mismatch in response frame");
        return -1;
    }

    *out_payload = payload;
    return 0;
}

plinth_client_t *plinth_connect(const char *host, int port) {
    plinth_crc32c_init();

    plinth_client_t *c = (plinth_client_t *)calloc(1, sizeof(plinth_client_t));
    if (!c) return NULL;

    c->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (c->sockfd < 0) {
        set_error(c, strerror(errno));
        free(c);
        return NULL;
    }

    int one = 1;
    setsockopt(c->sockfd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons((uint16_t)port);

    if (inet_pton(AF_INET, host, &serv_addr.sin_addr) <= 0) {
        set_error(c, "Invalid address / Address not supported");
        close(c->sockfd);
        free(c);
        return NULL;
    }

    if (connect(c->sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        set_error(c, strerror(errno));
        close(c->sockfd);
        free(c);
        return NULL;
    }

    return c;
}

void plinth_disconnect(plinth_client_t *client) {
    if (client) {
        if (client->sockfd >= 0) {
            close(client->sockfd);
        }
        free(client);
    }
}

int plinth_ping(plinth_client_t *client) {
    if (!client) return -1;
    if (send_frame(client, PLINTH_OP_PING, NULL, 0) != 0) return -1;

    uint8_t opcode = 0;
    uint8_t *payload = NULL;
    uint32_t len = 0;
    if (recv_frame(client, &opcode, &payload, &len) != 0) return -1;

    if (payload) free(payload);
    return (opcode == PLINTH_OP_PONG) ? 0 : -1;
}

int plinth_exec_sql(plinth_client_t *client, const char *sql, char *out_buf, size_t out_len) {
    if (!client || !sql) return -1;
    uint32_t sql_len = (uint32_t)strlen(sql);
    if (send_frame(client, PLINTH_OP_EXEC_SQL, sql, sql_len) != 0) return -1;

    uint8_t opcode = 0;
    uint8_t *payload = NULL;
    uint32_t len = 0;
    if (recv_frame(client, &opcode, &payload, &len) != 0) return -1;

    if (out_buf && out_len > 0) {
        if (payload) {
            strncpy(out_buf, (char *)payload, out_len - 1);
            out_buf[out_len - 1] = '\0';
        } else {
            out_buf[0] = '\0';
        }
    }
    if (payload) free(payload);
    return (opcode == PLINTH_OP_ACK) ? 0 : -1;
}

int plinth_ingest_row(plinth_client_t *client, const char *stream_name, const char *row_csv) {
    if (!client || !stream_name || !row_csv) return -1;
    char buffer[1024];
    int n = snprintf(buffer, sizeof(buffer), "%s\n%s", stream_name, row_csv);
    if (n < 0 || (size_t)n >= sizeof(buffer)) {
        set_error(client, "Payload too large");
        return -1;
    }
    if (send_frame(client, PLINTH_OP_INGEST, buffer, (uint32_t)n) != 0) return -1;

    uint8_t opcode = 0;
    uint8_t *payload = NULL;
    uint32_t len = 0;
    if (recv_frame(client, &opcode, &payload, &len) != 0) return -1;

    if (payload) free(payload);
    return (opcode == PLINTH_OP_ACK) ? 0 : -1;
}

int plinth_query_view(plinth_client_t *client, const char *view_name, char *out_buf, size_t out_len) {
    if (!client || !view_name) return -1;
    uint32_t name_len = (uint32_t)strlen(view_name);
    if (send_frame(client, PLINTH_OP_QUERY_VIEW, view_name, name_len) != 0) return -1;

    uint8_t opcode = 0;
    uint8_t *payload = NULL;
    uint32_t len = 0;
    if (recv_frame(client, &opcode, &payload, &len) != 0) return -1;

    if (out_buf && out_len > 0) {
        if (payload) {
            strncpy(out_buf, (char *)payload, out_len - 1);
            out_buf[out_len - 1] = '\0';
        } else {
            out_buf[0] = '\0';
        }
    }
    if (payload) free(payload);
    return (opcode == PLINTH_OP_ACK) ? 0 : -1;
}

int plinth_subscribe(plinth_client_t *client, const char *view_name) {
    if (!client || !view_name) return -1;
    uint32_t name_len = (uint32_t)strlen(view_name);
    if (send_frame(client, PLINTH_OP_SUB, view_name, name_len) != 0) return -1;

    uint8_t opcode = 0;
    uint8_t *payload = NULL;
    uint32_t len = 0;
    if (recv_frame(client, &opcode, &payload, &len) != 0) return -1;

    if (payload) free(payload);
    return (opcode == PLINTH_OP_ACK) ? 0 : -1;
}

const char *plinth_last_error(plinth_client_t *client) {
    return client ? client->error_buf : "Null client handle";
}
