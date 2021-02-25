/**
 * Plinth High-Performance Streaming Relational Engine
 *
 * test_client.c - Unit and correctness tests for C client and native vector routines.
 */

#include "plinth.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

static void test_crc32c_vectors(void) {
    printf("[TEST] CRC-32C Test Vectors...\n");
    plinth_crc32c_init();

    /* Standard check: ASCII "123456789" (9 bytes) has CRC32C = 0xE3069283 */
    const char *test1 = "123456789";
    uint32_t crc1 = plinth_crc32c(0, test1, 9);
    assert(crc1 == 0xE3069283u);

    /* 32 bytes of 0x00 */
    uint8_t zeros[32] = {0};
    uint32_t crc_zeros = plinth_crc32c(0, zeros, 32);
    assert(crc_zeros == 0x8A9136AAu);

    /* Incremental / chunked equivalence */
    uint32_t crc_part1 = plinth_crc32c(0, "12345", 5);
    uint32_t crc_combined = plinth_crc32c(crc_part1, "6789", 4);
    assert(crc_combined == 0xE3069283u);

    printf("  [PASS] CRC-32C Castagnoli hardware/software vectors match specification.\n");
}

static void test_vector_filtering_and_aggregates(void) {
    printf("[TEST] Native Vector Filtering and Reductions...\n");

    int64_t i_data[8] = {10, 25, 42, 5, 99, 100, 3, 77};
    uint32_t indices[8];

    /* Filter i_data > 40 */
    int matched = plinth_vector_filter_int64(i_data, 8, 40, PLINTH_CMP_GT, indices);
    assert(matched == 4);
    assert(indices[0] == 2); /* 42 */
    assert(indices[1] == 4); /* 99 */
    assert(indices[2] == 5); /* 100 */
    assert(indices[3] == 7); /* 77 */

    /* Gather matched */
    int64_t gathered[4];
    plinth_vector_gather_int64(i_data, indices, matched, gathered);
    assert(gathered[0] == 42);
    assert(gathered[1] == 99);
    assert(gathered[2] == 100);
    assert(gathered[3] == 77);

    /* Sum */
    int64_t sum_i = plinth_vector_sum_int64(i_data, 8);
    assert(sum_i == (10 + 25 + 42 + 5 + 99 + 100 + 3 + 77));

    /* Double vector operations */
    double d_data[6] = {10.5, 20.0, 5.25, 99.5, 12.0, 8.75};
    double d_sum = plinth_vector_sum_double(d_data, 6);
    assert(fabs(d_sum - 156.0) < 1e-9);

    double d_min = plinth_vector_min_double(d_data, 6);
    assert(fabs(d_min - 5.25) < 1e-9);

    double d_max = plinth_vector_max_double(d_data, 6);
    assert(fabs(d_max - 99.5) < 1e-9);

    matched = plinth_vector_filter_double(d_data, 6, 15.0, PLINTH_CMP_LT, indices);
    assert(matched == 4); /* 10.5, 5.25, 12.0, 8.75 */

    printf("  [PASS] Vector filtering, gather, and aggregate reductions verified.\n");
}

int main(void) {
    printf("=======================================================\n");
    printf("  Plinth Native C Test Suite\n");
    printf("=======================================================\n");
    test_crc32c_vectors();
    test_vector_filtering_and_aggregates();
    printf("All Native C tests PASSED successfully.\n");
    return 0;
}
