"""
Plinth high-level client interface for continuous streaming queries and IVM views.
"""

from typing import List, Dict, Any, Optional
from .connection import Connection
from .protocol import Frame, OpCode


class QueryError(Exception):
    pass


class PlinthClient:
    def __init__(self, host: str = "127.0.0.1", port: int = 8443, timeout: float = 10.0):
        self.conn = Connection(host=host, port=port, timeout=timeout)

    def connect(self) -> "PlinthClient":
        self.conn.connect()
        return self

    def close(self):
        self.conn.close()

    def __enter__(self) -> "PlinthClient":
        return self.connect()

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()

    def ping(self) -> bool:
        """Send a PING heartbeat frame and verify PONG reply."""
        frame = Frame(
            opcode=OpCode.PING,
            flags=0,
            corr_id=self.conn.next_corr_id(),
            payload=b"",
        )
        self.conn.send_frame(frame)
        resp = self.conn.recv_frame()
        return resp.opcode == OpCode.PONG

    def execute_sql(self, sql: str) -> str:
        """Execute a continuous streaming SQL query or DDL statement."""
        payload = sql.strip().encode("utf-8")
        frame = Frame(
            opcode=OpCode.EXEC_SQL,
            flags=0,
            corr_id=self.conn.next_corr_id(),
            payload=payload,
        )
        self.conn.send_frame(frame)
        resp = self.conn.recv_frame()
        text = resp.payload.decode("utf-8", errors="replace")
        if resp.opcode == OpCode.ERR:
            raise QueryError(f"SQL execution failed: {text}")
        return text

    def ingest(self, stream_name: str, rows: List[List[Any]]) -> int:
        """Ingest a batch of raw records into a named continuous stream."""
        lines = [stream_name]
        for row in rows:
            line = ",".join(str(v) for v in row)
            lines.append(line)
        payload = "\n".join(lines).encode("utf-8")
        frame = Frame(
            opcode=OpCode.INGEST,
            flags=0,
            corr_id=self.conn.next_corr_id(),
            payload=payload,
        )
        self.conn.send_frame(frame)
        resp = self.conn.recv_frame()
        if resp.opcode == OpCode.ERR:
            msg = resp.payload.decode("utf-8", errors="replace")
            raise QueryError(f"Ingestion failed: {msg}")
        return len(rows)

    def query_view(self, view_name: str) -> List[str]:
        """Query snapshot rows from an active continuous materialized view."""
        payload = view_name.strip().encode("utf-8")
        frame = Frame(
            opcode=OpCode.QUERY_VIEW,
            flags=0,
            corr_id=self.conn.next_corr_id(),
            payload=payload,
        )
        self.conn.send_frame(frame)
        resp = self.conn.recv_frame()
        if resp.opcode == OpCode.ERR:
            msg = resp.payload.decode("utf-8", errors="replace")
            raise QueryError(f"Query view failed: {msg}")
        raw = resp.payload.decode("utf-8", errors="replace").strip()
        if not raw:
            return []
        return [line for line in raw.split("\n") if line.strip()]

    def subscribe(self, view_name: str) -> str:
        """Subscribe to real-time differential change streams for a view."""
        payload = view_name.strip().encode("utf-8")
        frame = Frame(
            opcode=OpCode.SUB,
            flags=0,
            corr_id=self.conn.next_corr_id(),
            payload=payload,
        )
        self.conn.send_frame(frame)
        resp = self.conn.recv_frame()
        if resp.opcode == OpCode.ERR:
            msg = resp.payload.decode("utf-8", errors="replace")
            raise QueryError(f"Subscription failed: {msg}")
        return resp.payload.decode("utf-8", errors="replace")
