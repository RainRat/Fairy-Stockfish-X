#!/usr/bin/env python3
"""Tiny deterministic AEI fixture for the tournament runner smoke test."""

import sys


def main() -> int:
    for raw in sys.stdin:
        command = raw.rstrip("\r\n")
        if command == "aei":
            print("protocol-version 1", flush=True)
            print("id name fake-aei", flush=True)
            print("id author FSX tests", flush=True)
            print("aeiok", flush=True)
        elif command == "isready":
            print("readyok", flush=True)
        elif command == "go":
            # This is legal from the fixed standard setup and is sufficient to
            # exercise the complete-turn translation path.
            print("bestmove ra7s", flush=True)
        elif command == "quit":
            return 0
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
