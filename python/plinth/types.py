"""
Plinth data types, field definitions, and schema structures.
"""

from dataclasses import dataclass
from enum import IntEnum
from typing import List, Optional, Any


class DataType(IntEnum):
    BOOLEAN = 1
    INT32 = 2
    INT64 = 3
    FLOAT64 = 4
    STRING = 5
    TIMESTAMP = 6


@dataclass(frozen=True)
class Field:
    name: str
    data_type: DataType
    nullable: bool = False


@dataclass(frozen=True)
class Schema:
    fields: List[Field]

    @property
    def field_names(self) -> List[str]:
        return [f.name for f in self.fields]

    def find_index(self, name: str) -> Optional[int]:
        for idx, f in enumerate(self.fields):
            if f.name == name:
                return idx
        return None


@dataclass(frozen=True)
class RowId:
    partition: int
    offset: int


@dataclass(frozen=True)
class Timestamp:
    epoch_millis: int


def parse_scalar(type_tag: int, raw: str) -> Any:
    """Parse raw string token into Python typed value according to Plinth type tag."""
    if raw.lower() == "null":
        return None
    if type_tag == DataType.BOOLEAN:
        return raw.lower() in ("true", "1", "t", "yes")
    elif type_tag == DataType.INT32:
        return int(raw)
    elif type_tag == DataType.INT64:
        return int(raw)
    elif type_tag == DataType.FLOAT64:
        return float(raw)
    elif type_tag == DataType.STRING:
        return raw
    elif type_tag == DataType.TIMESTAMP:
        return int(raw)
    return raw
