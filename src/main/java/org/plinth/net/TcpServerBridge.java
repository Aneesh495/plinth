package org.plinth.net;

import java.io.IOException;
import java.net.InetSocketAddress;
import java.nio.ByteBuffer;
import java.nio.channels.SelectionKey;
import java.nio.channels.Selector;
import java.nio.channels.ServerSocketChannel;
import java.nio.channels.SocketChannel;
import java.nio.charset.StandardCharsets;
import java.util.Iterator;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.function.BiConsumer;

/**
 * High-performance Java NIO Asynchronous TCP Server Bridge.
 * Binds non-blocking sockets, accepts connections, and routes raw framed bytes to Frege handlers.
 */
public final class TcpServerBridge implements Runnable, AutoCloseable {

    private final int port;
    private final BiConsumer<Long, String> frameHandler;
    private final AtomicBoolean running = new AtomicBoolean(false);
    private ServerSocketChannel serverChannel;
    private Selector selector;
    private final ConcurrentHashMap<Long, SocketChannel> clientSessions = new ConcurrentHashMap<>();
    private long sessionCounter = 1L;

    public TcpServerBridge(int port, BiConsumer<Long, String> frameHandler) {
        this.port = port;
        this.frameHandler = frameHandler;
    }

    public synchronized void start() throws IOException {
        if (running.compareAndSet(false, true)) {
            this.selector = Selector.open();
            this.serverChannel = ServerSocketChannel.open();
            this.serverChannel.configureBlocking(false);
            this.serverChannel.bind(new InetSocketAddress(port));
            this.serverChannel.register(selector, SelectionKey.OP_ACCEPT);

            Thread serverThread = new Thread(this, "plinth-nio-server-" + port);
            serverThread.setDaemon(true);
            serverThread.start();
        }
    }

    @Override
    public void run() {
        ByteBuffer readBuffer = ByteBuffer.allocateDirect(65536);

        while (running.get()) {
            try {
                if (selector.select(100) == 0) continue;

                Iterator<SelectionKey> keyIter = selector.selectedKeys().iterator();
                while (keyIter.hasNext()) {
                    SelectionKey key = keyIter.next();
                    keyIter.remove();

                    if (!key.isValid()) continue;

                    if (key.isAcceptable()) {
                        handleAccept(key);
                    } else if (key.isReadable()) {
                        handleRead(key, readBuffer);
                    }
                }
            } catch (IOException e) {
                if (running.get()) {
                    System.err.println("[Plinth-NIO] Selector loop exception: " + e.getMessage());
                }
            }
        }
    }

    private void handleAccept(SelectionKey key) throws IOException {
        ServerSocketChannel ssc = (ServerSocketChannel) key.channel();
        SocketChannel client = ssc.accept();
        if (client != null) {
            client.configureBlocking(false);
            long sid = sessionCounter++;
            client.register(selector, SelectionKey.OP_READ, sid);
            clientSessions.put(sid, client);
        }
    }

    private void handleRead(SelectionKey key, ByteBuffer buffer) {
        SocketChannel client = (SocketChannel) key.channel();
        long sid = (Long) key.attachment();

        buffer.clear();
        int bytesRead;
        try {
            bytesRead = client.read(buffer);
        } catch (IOException e) {
            closeSession(sid, client);
            return;
        }

        if (bytesRead == -1) {
            closeSession(sid, client);
            return;
        }

        if (bytesRead > 0) {
            buffer.flip();
            byte[] bytes = new byte[bytesRead];
            buffer.get(bytes);
            String payload = new String(bytes, StandardCharsets.UTF_8);
            if (frameHandler != null) {
                frameHandler.accept(sid, payload);
            }
        }
    }

    /**
     * Send payload back to specific connected client session.
     */
    public boolean sendToSession(long sessionId, String message) {
        SocketChannel client = clientSessions.get(sessionId);
        if (client == null || !client.isOpen()) return false;

        try {
            byte[] bytes = message.getBytes(StandardCharsets.UTF_8);
            ByteBuffer buf = ByteBuffer.wrap(bytes);
            while (buf.hasRemaining()) {
                client.write(buf);
            }
            return true;
        } catch (IOException e) {
            closeSession(sessionId, client);
            return false;
        }
    }

    private void closeSession(long sessionId, SocketChannel client) {
        clientSessions.remove(sessionId);
        try {
            client.close();
        } catch (IOException ignored) {}
    }

    @Override
    public synchronized void close() {
        if (running.compareAndSet(true, false)) {
            try {
                if (selector != null) selector.close();
                if (serverChannel != null) serverChannel.close();
                for (SocketChannel sc : clientSessions.values()) {
                    try { sc.close(); } catch (IOException ignored) {}
                }
                clientSessions.clear();
            } catch (IOException ignored) {}
        }
    }
}
