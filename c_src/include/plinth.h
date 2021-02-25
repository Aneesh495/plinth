/**
 * Plinth High-Performance Streaming Relational Engine
 *
 * plinth.h - C Client SDK and Native Vector Execution Routines.
 */

#ifndef PLINTH_H
#define PLINTH_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "plinth_crc32c.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Binary Wire Protocol Constants */
#define PLINTH_MAGIC           0x504C4E31u  /* 'PLN1' */
#define PLINTH_PROTOCOL_VER    1
#define PLINTH_HEADER_SIZE     16

/* OpCodes */
#define PLINTH_OP_PING         0x01
#define PLINTH_OP_PONG         0x02
#define PLINTH_OP_EXEC_SQL     0x10
#define PLINTH_OP_INGEST       0x11
#define PLINTH_OP_SUB          0x12
#define PLINTH_OP_UNSUB        0x13
#define PLINTH_OP_QUERY_VIEW   0x14
#define PLINTH_OP_SCHEMA       0x20
#define PLINTH_OP_ROW_BATCH    0x21
#define PLINTH_OP_DELTA        0x22
#define PLINTH_OP_ACK          0x30
#define PLINTH_OP_ERR          0x31

/* Logical Type Tags */
#define PLINTH_TYPE_BOOL       1
#define PLINTH_TYPE_INT32      2
#define PLINTH_TYPE_INT64      3
#define PLINTH_TYPE_FLOAT64    4
#define PLINTH_TYPE_STRING     5
#define PLINTH_TYPE_TIMESTAMP  6

/* Comparison Operators for Vector Filtering */
#define PLINTH_CMP_EQ          0
#define PLINTH_CMP_NEQ         1
#define PLINTH_CMP_LT          2
#define PLINTH_CMP_LTE         3
#define PLINTH_CMP_GT          4
#define PLINTH_CMP_GTE         5

/* Columnar Vector Data Structure */
typedef struct {
    uint8_t type_tag;
    uint32_t count;
    uint32_t null_count;
    uint8_t *validity_bitmap;
    void *data;
} plinth_column_t;

/* Columnar Record Batch */
typedef struct {
    uint32_t num_columns;
    uint32_t row_count;
    plinth_column_t *columns;
} plinth_batch_t;

/* Opaque Client Handle */
typedef struct plinth_client plinth_client_t;

/* -------------------------------------------------------------------------
 * Native Vector Computation & SIMD Routines
 * ------------------------------------------------------------------------- */

int plinth_vector_filter_int64(const int64_t *src, int count, int64_t threshold,
                              int op, uint32_t *out_indices);

int plinth_vector_filter_double(const double *src, int count, double threshold,
                               int op, uint32_t *out_indices);

void plinth_vector_gather_int64(const int64_t *src, const uint32_t *indices,
                               int count, int64_t *dst);

void plinth_vector_gather_double(const double *src, const uint32_t *indices,
                                int count, double *dst);

int64_t plinth_vector_sum_int64(const int64_t *src, int count);

double plinth_vector_sum_double(const double *src, int count);

double plinth_vector_min_double(const double *src, int count);

double plinth_vector_max_double(const double *src, int count);

/* -------------------------------------------------------------------------
 * Client Network API
 * ------------------------------------------------------------------------- */

plinth_client_t *plinth_connect(const char *host, int port);

void plinth_disconnect(plinth_client_t *client);

int plinth_ping(plinth_client_t *client);

int plinth_exec_sql(plinth_client_t *client, const char *sql,
                    char *out_buf, size_t out_len);

int plinth_ingest_row(plinth_client_t *client, const char *stream_name,
                      const char *row_csv);

int plinth_query_view(plinth_client_t *client, const char *view_name,
                      char *out_buf, size_t out_len);

int plinth_subscribe(plinth_client_t *client, const char *view_name);

const char *plinth_last_error(plinth_client_t *client);

#ifdef __cplusplus
}
#endif

#endif /* PLINTH_H */
