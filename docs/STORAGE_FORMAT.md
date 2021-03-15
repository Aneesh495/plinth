# Plinth Columnar Storage and Segment Format

Plinth implements an append-only log-structured merge storage subsystem optimized for streaming workloads with mixed insertion, retraction, and range-scan patterns.

---

## 1. Storage Architecture Overview

```
Stream Event Ingestion
         │
         ├──► [1] Write-Ahead Log (WAL) ──► Append-Only Disk Persistence (CRC-32C)
         │
         └──► [2] MemTable ──────────────► Concurrent In-Memory Sorted Buffer
                     │
                     ▼ (Byte-size or row threshold reached)
              [3] Flush to SSTable
                     │
                     ▼
         ┌──────────────────────────────────────────────────┐
         │ SSTable Segment (Immutable Columnar Disk File)   │
         │  - Header & Schema Descriptor                    │
         │  - Encoded Column Blocks (RLE, Dict, Delta)     │
         │  - Two-Level Sparse Primary Key Index            │
         │  - Kirsch-Mitzenmacher Bloom Filter Block        │
         │  - Footer & Segment CRC-32C Trailer              │
         └──────────────────────────────────────────────────┘
                     │
                     ▼
              [4] Tiered Compaction & Dead Retraction Purging
```

---

## 2. Write-Ahead Log (WAL) Format

The Write-Ahead Log ensures durability before changes propagate through the continuous query dataflow graph. Every WAL record is framed with a 16-byte header:

- **Magic (4 bytes)**: `0x504C4E57` (`"PLNW"`).
- **Log Sequence Number (LSN, 8 bytes)**: Monotonically increasing 64-bit sequence identifier.
- **Timestamp (8 bytes)**: Nanosecond epoch timestamp of ingestion.
- **Record Type (1 byte)**:
  - `0x01`: Insert Event.
  - `0x02`: Retraction Event.
  - `0x03`: Stream DDL Definition.
  - `0x04`: Checkpoint Marker.
- **Stream Name Length (2 bytes)**: Length of stream identifier string.
- **Payload Length (4 bytes)**: Length of encoded tuple data.
- **CRC-32C Checksum (4 bytes)**: Castagnoli polynomial validation over header and payload.

---

## 3. Columnar SSTable Segment Layout

When a MemTable reaches its size threshold (typically 64 MB), it is frozen and written to disk as an immutable columnar SSTable segment file (`.sst`).

```
+---------------------------------------------------------------+
|                      Segment Magic (PLNS)                     |
+---------------------------------------------------------------+
|         Segment ID (8B)        |        Row Count (4B)        |
+---------------------------------------------------------------+
|                    Stream Schema Descriptor                   |
+---------------------------------------------------------------+
|                        Block Directory                        |
|  - Column 0: Offset, Length, Codec (RLE/Dict/Delta/Plain)     |
|  - Column 1: Offset, Length, Codec                            |
|  - ...                                                        |
+---------------------------------------------------------------+
|                      Column Data Block 0                      |
|  [Validity Bitmap] + [Encoded Column Primitive Payloads]      |
+---------------------------------------------------------------+
|                      Column Data Block 1                      |
+---------------------------------------------------------------+
|                    Two-Level Sparse Index                     |
|  [Sampled Primary Key Values -> Data Block Block Offsets]     |
+---------------------------------------------------------------+
|                      Bloom Filter Block                       |
|  [Murmur3 Double-Hash Bitset for Fast Key Pruning]            |
+---------------------------------------------------------------+
|                Footer & Segment CRC-32C (4B)                  |
+---------------------------------------------------------------+
```

### Column Encoding Schemes
1. **Plain**: Raw uncompressed contiguous primitives (aligned for SIMD load).
2. **Run-Length Encoding (RLE)**: Stores sequence of `(value, count)` pairs. Highly effective for low-cardinality status flags and categorical symbols.
3. **Dictionary Encoding**: Stores unique values in a contiguous dictionary array followed by compact bit-packed integer indices into the dictionary.
4. **Delta-of-Delta Encoding**: Stores initial base value followed by differences between consecutive values ($\Delta_i = x_i - x_{i-1}$). Ideal for ordered timestamps and sequence counters.

---

## 4. Tiered Compaction & Dead Retraction Purging

As new SSTables are flushed, background compaction runs across tiered levels:
- **Size-Tiered Leveling**: Segments of comparable sizes are merged into larger segments, converting multiple random reads into a single sequential scan.
- **Retraction Elimination**: In differential stream processing, an insertion $(x, +1)$ followed by a retraction $(x, -1)$ yields net weight 0. During compaction, tuples with zero net weight are permanently purged from disk storage, reclaiming capacity.
- **Sparse Index Reconstruction**: The compacted output segment receives a newly synthesized two-level sparse index and regenerated Bloom filter.
