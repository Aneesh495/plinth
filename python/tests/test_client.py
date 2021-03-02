"""
Unit tests for Plinth Python client and type conversions.
"""

import unittest
from plinth.types import DataType, Field, Schema, parse_scalar


class TestTypesAndClient(unittest.TestCase):
    def test_schema_field_indexing(self):
        s = Schema(
            fields=[
                Field("symbol", DataType.STRING),
                Field("price", DataType.FLOAT64),
                Field("volume", DataType.INT64),
            ]
        )
        self.assertEqual(s.field_names, ["symbol", "price", "volume"])
        self.assertEqual(s.find_index("price"), 1)
        self.assertEqual(s.find_index("nonexistent"), None)

    def test_parse_scalar(self):
        self.assertEqual(parse_scalar(DataType.BOOLEAN, "true"), True)
        self.assertEqual(parse_scalar(DataType.BOOLEAN, "false"), False)
        self.assertEqual(parse_scalar(DataType.INT32, "1234"), 1234)
        self.assertEqual(parse_scalar(DataType.INT64, "9876543210"), 9876543210)
        self.assertAlmostEqual(parse_scalar(DataType.FLOAT64, "3.14159"), 3.14159)
        self.assertEqual(parse_scalar(DataType.STRING, "AAPL"), "AAPL")
        self.assertIsNone(parse_scalar(DataType.INT32, "null"))


if __name__ == "__main__":
    unittest.main()
