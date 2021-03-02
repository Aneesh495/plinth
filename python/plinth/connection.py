"""
Plinth TCP connection management with binary frame framing and streaming I/O.
"""

import socket
import struct
from typing import Optional
from .protocol import Frame, HEADER_SIZE, crc32c


class ConnectionError(Exception):
    pass


class Connection:
    def __init__(self, host: str = "127.0.0.1", port: int = 8443, timeout: float = 10.0):
        self.host = host
        self.port = port
        self.timeout = timeout
        self._sock: Optional[socket.socket] = None
        self._corr_id = 0

    def connect(self) -> "Connection":
        if self._sock is not None:
            return self
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            sock.settimeout(self.timeout)
            sock.connect((self.host, self.port))
            self._sock = sock
            return self
        except Exception as e:
            raise ConnectionError(f"Failed to connect to {self.host}:{self.port}: {e}") from e

    def close(self):
        if self._sock is not None:
            try:
                self._sock.close()
            except Exception:
                pass
            finally:
                self._sock = None

    def __enter__(self) -> "Connection":
        return self.connect()

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()

    def _send_all(self, data: bytes):
        if self._sock is None:
            raise ConnectionError("Not connected to server")
        total_sent = 0
        while total_sent < len(data):
            sent = self._sock.send(data[total_sent:])
            if sent == 0:
                raise ConnectionError("Socket connection broken on send")
            total_sent += sent

    def _recv_exact(self, count: int) -> bytes:
        if self._sock is None:
            raise ConnectionError("Not connected to server")
        buf = bytearray()
        while len(buf) < count:
            chunk = self._sock.recv(count - len(buf))
            if not chunk:
                raise ConnectionError("Socket connection closed by remote peer")
            buf.extend(chunk)
        return bytes(buf)

    def next_corr_id(self) -> int:
        self._corr_id = (self._corr_id + 1) & 0x7FFFFFFF
        return self._corr_id

    def send_frame(self, frame: Frame):
        self._send_all(frame.encode())

    def recv_frame(self) -> Frame:
        header_bytes = self._recv_exact(HEADER_SIZE)
        magic, ver, opcode, flags, corr_id, payload_len = Frame.decode_header(header_bytes)

        payload_bytes = self._recv_exact(payload_len) if payload_len > 0 else b""
        crc_bytes = self._recv_exact(4)
        expected_crc = struct.unpack(">I", crc_bytes)[0]

        actual_crc = crc32c(header_bytes)
        if payload_bytes:
            actual_crc = crc32c(payload_bytes, actual_crc)

        if actual_crc != expected_crc:
            raise ConnectionError(
                f"CRC32C mismatch on frame {corr_id}: expected {hex(expected_crc)}, got {hex(actual_crc)}"
            )

        return Frame(opcode=opcode, flags=flags, corr_id=corr_id, payload=payload_bytes)
