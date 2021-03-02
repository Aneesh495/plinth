"""
Unit tests for Plinth PLN1 binary wire protocol framing and CRC-32C verification.
"""

import unittest
from plinth.protocol import crc32c, Frame, OpCode, MAGIC, PROTOCOL_VERSION


class TestProtocol(unittest.TestCase):
    def test_crc32c_standard_vectors(self):
        # Castagnoli standard test vector
        self.assertEqual(crc32c(b"123456789"), 0xE3069283)
        self.assertEqual(crc32c(b"\x00" * 32), 0x8A9136AA)

        # Multi-part chunked calculation equivalence
        c1 = crc32c(b"12345")
        c2 = crc32c(b"6789", c1)
        self.assertEqual(c2, 0xE3069283)

    def test_frame_encode_decode(self):
        payload = b"SELECT * FROM trades EMIT CHANGES;"
        frame = Frame(
            opcode=OpCode.EXEC_SQL,
            flags=0,
            corr_id=42,
            payload=payload,
        )
        encoded = frame.encode()
        self.assertGreater(len(encoded), 16 + len(payload))

        header_bytes = encoded[:16]
        magic, ver, op, flags, corr_id, payload_len = Frame.decode_header(header_bytes)
        self.assertEqual(magic, MAGIC)
        self.assertEqual(ver, PROTOCOL_VERSION)
        self.assertEqual(op, OpCode.EXEC_SQL)
        self.assertEqual(flags, 0)
        self.assertEqual(corr_id, 42)
        self.assertEqual(payload_len, len(payload))

    def test_frame_corrupt_magic(self):
        bad_header = b"BAD1" + b"\x00" * 12
        with self.assertRaises(ValueError):
            Frame.decode_header(bad_header)


if __name__ == "__main__":
    unittest.main()
