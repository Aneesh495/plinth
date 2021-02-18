package org.plinth.interop;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * Off-heap direct memory buffer providing zero-copy primitives for columnar chunks.
 */
public final class OffHeapBuffer implements AutoCloseable {

    private final ByteBuffer buffer;
    private final int capacity;
    private int position;

    public OffHeapBuffer(int capacityBytes) {
        this.capacity = capacityBytes;
        this.buffer = ByteBuffer.allocateDirect(capacityBytes).order(ByteOrder.nativeOrder());
        this.position = 0;
    }

    public static OffHeapBuffer allocate(int capacityBytes) {
        return new OffHeapBuffer(capacityBytes);
    }

    public int getCapacity() {
        return capacity;
    }

    public int getPosition() {
        return position;
    }

    public void setPosition(int newPos) {
        if (newPos >= 0 && newPos <= capacity) {
            this.position = newPos;
            this.buffer.position(newPos);
        }
    }

    public void putInt(int value) {
        ensureCapacity(4);
        buffer.putInt(position, value);
        position += 4;
    }

    public int getInt(int offset) {
        return buffer.getInt(offset);
    }

    public void putLong(long value) {
        ensureCapacity(8);
        buffer.putLong(position, value);
        position += 8;
    }

    public long getLong(int offset) {
        return buffer.getLong(offset);
    }

    public void putDouble(double value) {
        ensureCapacity(8);
        buffer.putDouble(position, value);
        position += 8;
    }

    public double getDouble(int offset) {
        return buffer.getDouble(offset);
    }

    public void putBytes(byte[] src, int srcOffset, int length) {
        ensureCapacity(length);
        int oldPos = buffer.position();
        buffer.position(position);
        buffer.put(src, srcOffset, length);
        buffer.position(oldPos);
        position += length;
    }

    public void getBytes(int offset, byte[] dst, int dstOffset, int length) {
        int oldPos = buffer.position();
        buffer.position(offset);
        buffer.get(dst, dstOffset, length);
        buffer.position(oldPos);
    }

    public ByteBuffer getUnderlyingBuffer() {
        return buffer.duplicate();
    }

    private void ensureCapacity(int additionalBytes) {
        if (position + additionalBytes > capacity) {
            throw new IndexOutOfBoundsException(
                "OffHeapBuffer overflow: pos=" + position + ", additional=" + additionalBytes + ", cap=" + capacity
            );
        }
    }

    @Override
    public void close() {
        // Direct buffer memory collected by JVM GC cleaner
    }
}
