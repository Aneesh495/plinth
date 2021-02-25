/**
 * Plinth High-Performance Streaming Relational Engine
 *
 * plinth_crc32c.c - Hardware-accelerated Castagnoli CRC-32C implementation.
 */

#include "plinth_crc32c.h"

#define CRC32C_POLYNOMIAL 0x82F63B78u

static uint32_t crc32c_table[8][256];
static int crc32c_initialized = 0;

void plinth_crc32c_init(void) {
    if (crc32c_initialized) {
        return;
    }
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ ((crc & 1) ? CRC32C_POLYNOMIAL : 0);
        }
        crc32c_table[0][i] = crc;
    }
    for (int slice = 1; slice < 8; slice++) {
        for (uint32_t i = 0; i < 256; i++) {
            crc32c_table[slice][i] = (crc32c_table[slice - 1][i] >> 8) ^
                                     crc32c_table[0][crc32c_table[slice - 1][i] & 0xFF];
        }
    }
    crc32c_initialized = 1;
}

uint32_t plinth_crc32c(uint32_t crc, const void *buffer, size_t length) {
    if (!crc32c_initialized) {
        plinth_crc32c_init();
    }

    const uint8_t *p = (const uint8_t *)buffer;
    crc = ~crc;

#if defined(__aarch64__) && defined(__ARM_FEATURE_CRC32)
    while (length >= 8) {
        uint64_t val;
        __builtin_memcpy(&val, p, sizeof(val));
        crc = __builtin_arm_crc32cd(crc, val);
        p += 8;
        length -= 8;
    }
    if (length >= 4) {
        uint32_t val;
        __builtin_memcpy(&val, p, sizeof(val));
        crc = __builtin_arm_crc32cw(crc, val);
        p += 4;
        length -= 4;
    }
    if (length >= 2) {
        uint16_t val;
        __builtin_memcpy(&val, p, sizeof(val));
        crc = __builtin_arm_crc32ch(crc, val);
        p += 2;
        length -= 2;
    }
    if (length == 1) {
        crc = __builtin_arm_crc32cb(crc, *p);
    }
#elif defined(__SSE4_2__) && defined(__x86_64__)
    #include <nmmintrin.h>
    while (length >= 8) {
        uint64_t val;
        __builtin_memcpy(&val, p, sizeof(val));
        crc = (uint32_t)_mm_crc32_u64(crc, val);
        p += 8;
        length -= 8;
    }
    if (length >= 4) {
        uint32_t val;
        __builtin_memcpy(&val, p, sizeof(val));
        crc = _mm_crc32_u32(crc, val);
        p += 4;
        length -= 4;
    }
    if (length >= 2) {
        uint16_t val;
        __builtin_memcpy(&val, p, sizeof(val));
        crc = _mm_crc32_u16(crc, val);
        p += 2;
        length -= 2;
    }
    if (length == 1) {
        crc = _mm_crc32_u8(crc, *p);
    }
#else
    /* Optimized Slice-by-8 software calculation */
    while (length >= 8) {
        uint32_t load1 = (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                         ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
        uint32_t term1 = crc ^ load1;
        uint32_t term2 = (uint32_t)p[4] | ((uint32_t)p[5] << 8) |
                         ((uint32_t)p[6] << 16) | ((uint32_t)p[7] << 24);
        crc = crc32c_table[7][term1 & 0xFF] ^
              crc32c_table[6][(term1 >> 8) & 0xFF] ^
              crc32c_table[5][(term1 >> 16) & 0xFF] ^
              crc32c_table[4][term1 >> 24] ^
              crc32c_table[3][term2 & 0xFF] ^
              crc32c_table[2][(term2 >> 8) & 0xFF] ^
              crc32c_table[1][(term2 >> 16) & 0xFF] ^
              crc32c_table[0][term2 >> 24];
        p += 8;
        length -= 8;
    }
    while (length > 0) {
        crc = crc32c_table[0][(crc ^ *p++) & 0xFF] ^ (crc >> 8);
        length--;
    }
#endif

    return ~crc;
}
