/**
 * Plinth High-Performance Streaming Relational Engine
 *
 * plinth_vector.c - Native vectorized filter, gather, and aggregate execution.
 */

#include "plinth.h"
#include <float.h>

int plinth_vector_filter_int64(const int64_t *src, int count, int64_t threshold,
                              int op, uint32_t *out_indices) {
    int matched = 0;
    for (int i = 0; i < count; i++) {
        int64_t val = src[i];
        bool ok = false;
        switch (op) {
            case PLINTH_CMP_EQ:  ok = (val == threshold); break;
            case PLINTH_CMP_NEQ: ok = (val != threshold); break;
            case PLINTH_CMP_LT:  ok = (val <  threshold); break;
            case PLINTH_CMP_LTE: ok = (val <= threshold); break;
            case PLINTH_CMP_GT:  ok = (val >  threshold); break;
            case PLINTH_CMP_GTE: ok = (val >= threshold); break;
            default: break;
        }
        if (ok) {
            out_indices[matched++] = (uint32_t)i;
        }
    }
    return matched;
}

int plinth_vector_filter_double(const double *src, int count, double threshold,
                               int op, uint32_t *out_indices) {
    int matched = 0;
    for (int i = 0; i < count; i++) {
        double val = src[i];
        bool ok = false;
        switch (op) {
            case PLINTH_CMP_EQ:  ok = (val == threshold); break;
            case PLINTH_CMP_NEQ: ok = (val != threshold); break;
            case PLINTH_CMP_LT:  ok = (val <  threshold); break;
            case PLINTH_CMP_LTE: ok = (val <= threshold); break;
            case PLINTH_CMP_GT:  ok = (val >  threshold); break;
            case PLINTH_CMP_GTE: ok = (val >= threshold); break;
            default: break;
        }
        if (ok) {
            out_indices[matched++] = (uint32_t)i;
        }
    }
    return matched;
}

void plinth_vector_gather_int64(const int64_t *src, const uint32_t *indices,
                               int count, int64_t *dst) {
    for (int i = 0; i < count; i++) {
        dst[i] = src[indices[i]];
    }
}

void plinth_vector_gather_double(const double *src, const uint32_t *indices,
                                int count, double *dst) {
    for (int i = 0; i < count; i++) {
        dst[i] = src[indices[i]];
    }
}

int64_t plinth_vector_sum_int64(const int64_t *src, int count) {
    int64_t sum = 0;
    for (int i = 0; i < count; i++) {
        sum += src[i];
    }
    return sum;
}

double plinth_vector_sum_double(const double *src, int count) {
    double sum = 0.0;
    for (int i = 0; i < count; i++) {
        sum += src[i];
    }
    return sum;
}

double plinth_vector_min_double(const double *src, int count) {
    if (count <= 0) return 0.0;
    double min_val = src[0];
    for (int i = 1; i < count; i++) {
        if (src[i] < min_val) {
            min_val = src[i];
        }
    }
    return min_val;
}

double plinth_vector_max_double(const double *src, int count) {
    if (count <= 0) return 0.0;
    double max_val = src[0];
    for (int i = 1; i < count; i++) {
        if (src[i] > max_val) {
            max_val = src[i];
        }
    }
    return max_val;
}
