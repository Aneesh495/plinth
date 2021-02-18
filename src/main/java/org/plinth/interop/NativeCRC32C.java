package org.plinth.interop;

import java.nio.ByteBuffer;
import java.util.zip.CRC32C;

/**
 * Hardware-accelerated Castagnoli CRC-32C implementation.
 * Leverages JVM intrinsics targeting CPU instructions on x86-64 (SSE4.2) and ARM64.
 */
public final class NativeCRC32C {

    private NativeCRC32C() {}

    /**
     * Compute 32-bit CRC-32C checksum of a byte array.
     */
    public static int compute(byte[] data, int offset, int length) {
        if (data == null || length <= 0) return 0;
        CRC32C crc = new CRC32C();
        crc.update(data, offset, length);
        return (int) crc.getValue();
    }

    /**
     * Compute 32-bit CRC-32C checksum of an entire byte array.
     */
    public static int computeAll(byte[] data) {
        if (data == null) return 0;
        return compute(data, 0, data.length);
    }

    /**
     * Compute CRC-32C checksum over a ByteBuffer.
     */
    public static int computeBuffer(ByteBuffer buffer, int offset, int length) {
        if (buffer == null || length <= 0) return 0;
        CRC32C crc = new CRC32C();
        int oldPos = buffer.position();
        int oldLim = buffer.limit();
        try {
            buffer.position(offset);
            buffer.limit(offset + length);
            crc.update(buffer);
            return (int) crc.getValue();
        } finally {
            buffer.limit(oldLim);
            buffer.position(oldPos);
        }
    }
}
