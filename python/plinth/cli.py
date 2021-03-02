"""
Plinth Interactive Command-Line Interface.
"""

import sys
import argparse
from .client import PlinthClient, QueryError


BANNER = r"""
==========================================================================
  PLINTH : Purely Functional Stream-Relational Engine
  Version 0.1.0 | DBSP Differential Stream Algebra | Columnar Vectors
==========================================================================
"""


def main():
    parser = argparse.ArgumentParser(description="Plinth Streaming SQL Shell")
    parser.add_argument("--host", default="127.0.0.1", help="Plinth server hostname")
    parser.add_argument("-p", "--port", type=int, default=8443, help="Plinth server TCP port")
    parser.add_argument("-e", "--execute", type=str, default=None, help="Execute single SQL statement and exit")
    args = parser.parse_args()

    client = PlinthClient(host=args.host, port=args.port)

    if args.execute:
        try:
            with client:
                res = client.execute_sql(args.execute)
                print(res)
        except Exception as e:
            print(f"Error: {e}", file=sys.stderr)
            sys.exit(1)
        return

    print(BANNER)
    print(f"Connecting to Plinth server at {args.host}:{args.port}...")

    try:
        client.connect()
        print("Connected. Type 'exit' to quit or 'help' for quick commands.\n")
    except Exception as e:
        print(f"Connection failed: {e}")
        print("Running in offline parsing verification mode.\n")

    while True:
        try:
            line = input("plinth> ").strip()
        except (EOFError, KeyboardInterrupt):
            print("\nBye.")
            break

        if not line:
            continue
        if line.lower() in ("exit", "quit"):
            print("Bye.")
            break
        if line.lower() == "ping":
            try:
                ok = client.ping()
                print("PONG" if ok else "PING failed")
            except Exception as e:
                print(f"Error: {e}")
            continue

        try:
            resp = client.execute_sql(line)
            print(resp)
        except Exception as e:
            print(f"Error: {e}")


if __name__ == "__main__":
    main()
