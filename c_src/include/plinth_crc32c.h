/**
 * Plinth High-Performance Streaming Relational Engine
 *
 * plinth_crc32c.h - Hardware-accelerated Castagnoli CRC-32C implementation.
 *
 * Uses ARM64 NEON/CRC instructions on Apple Silicon and SSE4.2 on x86_64,
 * with an optimized slice-by-8 lookup table fallback.
 */

#ifndef PLINTH_CRC32C_H
#define PLINTH_CRC32C_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize CRC-32C lookup tables for software fallback.
 * Safe to call multiple times; thread-safe initialization.
 */
void plinth_crc32c_init(void);

/**
 * Compute the Castagnoli CRC-32C checksum of a memory buffer.
 *
 * @param crc     Initial CRC value (typically 0 for new streams).
 * @param buffer  Pointer to input byte buffer.
 * @param length  Number of bytes to process.
 * @return Computed CRC-32C checksum.
 */
uint32_t plinth_crc32c(uint32_t crc, const void *buffer, size_t length);

#ifdef __cplusplus
}
#endif

#endif /* PLINTH_CRC32C_H */
