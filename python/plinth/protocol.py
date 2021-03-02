"""
Plinth PLN1 binary wire protocol framing, serialization, and CRC-32C verification.
"""

import struct
from dataclasses import dataclass
from enum import IntEnum
from typing import Optional, Tuple


MAGIC = 0x504C4E31  # ASCII 'PLN1'
PROTOCOL_VERSION = 1
HEADER_SIZE = 16


class OpCode(IntEnum):
    PING = 0x01
    PONG = 0x02
    EXEC_SQL = 0x10
    INGEST = 0x11
    SUB = 0x12
    UNSUB = 0x13
    QUERY_VIEW = 0x14
    SCHEMA = 0x20
    ROW_BATCH = 0x21
    DELTA = 0x22
    ACK = 0x30
    ERR = 0x31


# Precomputed Castagnoli CRC-32C lookup table (polynomial 0x82F63B78)
_CRC32C_TABLE = []
for _i in range(256):
    _c = _i
    for _ in range(8):
        _c = (_c >> 1) ^ (0x82F63B78 if (_c & 1) else 0)
    _CRC32C_TABLE.append(_c)


def crc32c(data: bytes, init: int = 0) -> int:
    """Compute 32-bit Castagnoli CRC-32C checksum over data bytes."""
    crc = init ^ 0xFFFFFFFF
    for b in data:
        crc = _CRC32C_TABLE[(crc ^ b) & 0xFF] ^ (crc >> 8)
    return (crc ^ 0xFFFFFFFF) & 0xFFFFFFFF


@dataclass(frozen=True)
class Frame:
    opcode: OpCode
    flags: int
    corr_id: int
    payload: bytes

    def encode(self) -> bytes:
        """Encode frame into wire format: Header (16B) + Payload + CRC32C (4B)."""
        header = struct.pack(
            ">IBBHII",
            MAGIC,
            PROTOCOL_VERSION,
            int(self.opcode),
            self.flags,
            self.corr_id,
            len(self.payload),
        )
        checksum = crc32c(header)
        if self.payload:
            checksum = crc32c(self.payload, checksum)
        trailer = struct.pack(">I", checksum)
        return header + self.payload + trailer

    @classmethod
    def decode_header(cls, header_bytes: bytes) -> Tuple[int, int, OpCode, int, int, int]:
        """Decode and validate a 16-byte PLN1 frame header."""
        if len(header_bytes) < HEADER_SIZE:
            raise ValueError(f"Header too short: expected {HEADER_SIZE} bytes, got {len(header_bytes)}")
        magic, ver, op, flags, corr_id, payload_len = struct.unpack(">IBBHII", header_bytes)
        if magic != MAGIC:
            raise ValueError(f"Invalid magic: expected {hex(MAGIC)}, got {hex(magic)}")
        if ver != PROTOCOL_VERSION:
            raise ValueError(f"Unsupported protocol version: {ver}")
        return magic, ver, OpCode(op), flags, corr_id, payload_len
