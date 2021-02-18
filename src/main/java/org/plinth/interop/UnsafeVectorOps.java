package org.plinth.interop;

/**
 * High-performance vector primitives optimized for SIMD auto-vectorization.
 */
public final class UnsafeVectorOps {

    private UnsafeVectorOps() {}

    /**
     * Vectorized pairwise addition for 64-bit integer arrays.
     */
    public static void addLongArrays(long[] a, long[] b, long[] result, int length) {
        int i = 0;
        int unrollLimit = length - (length % 4);
        for (; i < unrollLimit; i += 4) {
            result[i] = a[i] + b[i];
            result[i + 1] = a[i + 1] + b[i + 1];
            result[i + 2] = a[i + 2] + b[i + 2];
            result[i + 3] = a[i + 3] + b[i + 3];
        }
        for (; i < length; i++) {
            result[i] = a[i] + b[i];
        }
    }

    /**
     * Vectorized pairwise addition for 64-bit floating point arrays.
     */
    public static void addDoubleArrays(double[] a, double[] b, double[] result, int length) {
        int i = 0;
        int unrollLimit = length - (length % 4);
        for (; i < unrollLimit; i += 4) {
            result[i] = a[i] + b[i];
            result[i + 1] = a[i + 1] + b[i + 1];
            result[i + 2] = a[i + 2] + b[i + 2];
            result[i + 3] = a[i + 3] + b[i + 3];
        }
        for (; i < length; i++) {
            result[i] = a[i] + b[i];
        }
    }

    /**
     * Vectorized filter scanning: evaluates array == target and packs matches into 64-bit bitmap words.
     */
    public static void scanEqualsLong(long[] data, long target, long[] outBitsetWords, int length) {
        int wordIdx = 0;
        long currentWord = 0L;
        for (int i = 0; i < length; i++) {
            int bitIdx = i % 64;
            if (data[i] == target) {
                currentWord |= (1L << bitIdx);
            }
            if (bitIdx == 63 || i == length - 1) {
                outBitsetWords[wordIdx++] = currentWord;
                currentWord = 0L;
            }
        }
    }

    /**
     * Vectorized filter scanning: evaluates array > target.
     */
    public static void scanGreaterThanLong(long[] data, long threshold, long[] outBitsetWords, int length) {
        int wordIdx = 0;
        long currentWord = 0L;
        for (int i = 0; i < length; i++) {
            int bitIdx = i % 64;
            if (data[i] > threshold) {
                currentWord |= (1L << bitIdx);
            }
            if (bitIdx == 63 || i == length - 1) {
                outBitsetWords[wordIdx++] = currentWord;
                currentWord = 0L;
            }
        }
    }
}
