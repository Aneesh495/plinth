"""
Plinth High-Throughput Stream Ingestion and Continuous Query Stress Harness.
"""

import sys
import time
import argparse
from typing import List

# Ensure python client package is on path
import os
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "python")))

from plinth.client import PlinthClient, QueryError


def run_benchmark(host: str, port: int, event_count: int, batch_size: int):
    print("==========================================================================")
    print("  PLINTH CLIENT-SERVER STRESS HARNESS")
    print(f"  Target: {host}:{port} | Events: {event_count:,} | Batch Size: {batch_size:,}")
    print("==========================================================================")

    client = PlinthClient(host=host, port=port)
    try:
        client.connect()
    except Exception as e:
        print(f"Could not connect to {host}:{port}: {e}")
        print("Run 'bin/plinth-server' in another terminal to benchmark client-server mode.")
        return

    print("Connected. Verifying PING heartbeat...")
    if not client.ping():
        print("Heartbeat ping failed!")
        return
    print("Heartbeat PONG verified.")

    # 1. Define stream schema
    stream_ddl = "CREATE STREAM trades (symbol STRING NOT NULL, price DOUBLE, volume BIGINT);"
    print(f"\n[1] Registering stream: {stream_ddl}")
    try:
        client.execute_sql(stream_ddl)
    except QueryError as e:
        print(f"Notice: {e}")

    # 2. Deploy continuous aggregation query
    query = "SELECT symbol, COUNT(*) AS trade_count, SUM(price) AS volume FROM trades GROUP BY symbol EMIT CHANGES;"
    print(f"\n[2] Deploying continuous query: {query}")
    try:
        client.execute_sql(query)
    except QueryError as e:
        print(f"Notice: {e}")

    # 3. Stream batches of events
    symbols = ["AAPL", "GOOG", "MSFT", "AMZN", "META", "NVDA", "TSLA"]
    print(f"\n[3] Ingesting {event_count:,} stream events...")

    total_ingested = 0
    t0 = time.time()

    while total_ingested < event_count:
        cur_batch_size = min(batch_size, event_count - total_ingested)
        rows: List[List[object]] = []
        for i in range(cur_batch_size):
            sym = symbols[(total_ingested + i) % len(symbols)]
            price = 100.0 + ((total_ingested + i) % 500)
            vol = 10 + ((total_ingested + i) % 100)
            rows.append([sym, price, vol])

        client.ingest("trades", rows)
        total_ingested += cur_batch_size

    t1 = time.time()
    elapsed = t1 - t0
    rate = total_ingested / elapsed if elapsed > 0 else 0.0

    print(f"Ingested {total_ingested:,} events in {elapsed:.3f} sec ({rate:,.1f} events/sec).")

    # 4. Query continuous materialized view
    print("\n[4] Querying active state of continuous materialized view 'trades_stats'...")
    try:
        rows = client.query_view("trades_stats")
        print(f"Retrieved {len(rows)} grouped rows from continuous view:")
        for r in rows:
            print(f"  {r}")
    except Exception as e:
        print(f"Query view notice: {e}")

    print("\n==========================================================================")
    print("  STRESS BENCHMARK FINISHED SUCCESSFULLY")
    print("==========================================================================")


def main():
    parser = argparse.ArgumentParser(description="Plinth Ingestion and Query Stress Benchmark")
    parser.add_argument("--host", default="127.0.0.1", help="Plinth server hostname")
    parser.add_argument("-p", "--port", type=int, default=8443, help="Plinth server port")
    parser.add_argument("-n", "--count", type=int, default=50000, help="Total number of stream events")
    parser.add_argument("-b", "--batch-size", type=int, default=1000, help="Ingestion batch size")
    args = parser.parse_args()

    run_benchmark(host=args.host, port=args.port, event_count=args.count, batch_size=args.batch_size)


if __name__ == "__main__":
    main()
