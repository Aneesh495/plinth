# PLN1 Binary Wire Protocol Specification

The PLN1 wire protocol is a high-throughput, multiplexed binary protocol designed for low-latency continuous stream ingestion, query execution, and differential event subscription in Plinth.

---

## 1. Frame Structure

Every PLN1 frame consists of a fixed 16-byte header, an optional variable-length payload, and a mandatory 4-byte Castagnoli CRC-32C trailer. All multi-byte integer fields are encoded in **Big-Endian (Network Byte Order)**.

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       Magic (0x504C4E31)                      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|    Version    |     OpCode    |             Flags             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                        Correlation ID                         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                        Payload Length                         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
+                     Payload Data (N bytes)                    +
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                   CRC-32C Checksum (4 bytes)                  |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

### Field Definitions
- **Magic (32 bits)**: Fixed ASCII bytes `0x504C4E31` (`"PLN1"`). Frames with mismatched magic are immediately rejected and the TCP socket connection is terminated.
- **Version (8 bits)**: Protocol revision number. Current specification is `0x01`.
- **OpCode (8 bits)**: Operation identifier specifying frame intent and payload semantics.
- **Flags (16 bits)**: Bitmask for frame options:
  - Bit 0 (`0x0001`): Compressed payload (Snappy / LZ4).
  - Bit 1 (`0x0002`): End-of-Stream marker.
  - Bits 2-15: Reserved for future extensions (must be 0).
- **Correlation ID (32 bits)**: Monotonically increasing identifier matching asynchronous responses to client requests.
- **Payload Length (32 bits)**: Byte length of the following payload block (0 to 67,108,864 bytes / 64 MB maximum).
- **Payload Data**: Raw bytes corresponding to the specified OpCode.
- **CRC-32C Checksum (32 bits)**: Castagnoli polynomial (`0x82F63B78`) checksum calculated across the 16-byte header concatenated with the payload bytes.

---

## 2. OpCodes

| OpCode | Hex | Name | Direction | Description |
|---|---|---|---|---|
| 1 | `0x01` | `PING` | Client -> Server | Heartbeat check to verify connection liveliness |
| 2 | `0x02` | `PONG` | Server -> Client | Heartbeat response confirming connection health |
| 16 | `0x10` | `EXEC_SQL` | Client -> Server | Submit DDL or continuous SQL query |
| 17 | `0x11` | `INGEST` | Client -> Server | Ingest batch of raw events into a named stream |
| 18 | `0x12` | `SUB` | Client -> Server | Subscribe to differential changes from a view |
| 19 | `0x13` | `UNSUB` | Client -> Server | Cancel an active differential view subscription |
| 20 | `0x14` | `QUERY_VIEW`| Client -> Server | Request current snapshot state of a materialized view |
| 32 | `0x20` | `SCHEMA` | Bi-directional | Schema negotiation and column metadata |
| 33 | `0x21` | `ROW_BATCH` | Bi-directional | Columnar record batch payload |
| 34 | `0x22` | `DELTA` | Server -> Client | Real-time differential delta event ($Z$-Set) |
| 48 | `0x30` | `ACK` | Server -> Client | Operation success acknowledgement |
| 49 | `0x31` | `ERR` | Server -> Client | Operation failure error message |

---

## 3. Communication Patterns

### Request-Response Exchange
For synchronous commands such as `EXEC_SQL`, `INGEST`, and `QUERY_VIEW`, the client sends a request frame with a unique Correlation ID. The server processes the command and returns a response frame (`ACK` or `ERR`) matching the same Correlation ID.

### Streaming Subscriptions
When a client issues a `SUB` frame for a continuous view:
1. The server registers the client's TCP channel into the dataflow graph's subscriber registry.
2. Whenever upstream stream events cause the materialized view to step, the engine emits a `DELTA` frame containing the incremental insertion and retraction tuples.
3. Subscription streams continue until the client issues an `UNSUB` frame or closes the TCP connection.
