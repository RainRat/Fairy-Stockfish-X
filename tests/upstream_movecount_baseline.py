#!/usr/bin/env python3

import argparse
import json
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


MOVE_RE = re.compile(r"^bestmove\s+(\S+)")
FEN_RE = re.compile(r"^Fen:\s+(.*)$")
PERFT_RE = re.compile(r"^Nodes searched:\s+(\d+)\s*$")


@dataclass(frozen=True)
class BaselineSpec:
    name: str
    variant: str
    plies: int


SPECS = [
    BaselineSpec("chess", "chess", 8),
    BaselineSpec("berolina", "berolina", 8),
    BaselineSpec("seirawan", "seirawan", 8),
    BaselineSpec("torpedo", "torpedo", 8),
    BaselineSpec("atomic", "atomic", 8),
    BaselineSpec("allexplodeatomic", "allexplodeatomic", 8),
    BaselineSpec("duck", "duck", 6),
    BaselineSpec("spartan", "spartan", 8),
    BaselineSpec("racingkings", "racingkings", 8),
    BaselineSpec("xiangqi", "xiangqi", 8),
]


def run_uci(engine: Path, lines: list[str], timeout: int = 60) -> str:
    proc = subprocess.run(
        [str(engine)],
        input="\n".join(lines + ["quit", ""]),
        text=True,
        capture_output=True,
        check=False,
        timeout=timeout,
    )
    if proc.returncode != 0:
        raise RuntimeError(
            f"{engine} failed with code {proc.returncode}\nSTDOUT:\n{proc.stdout}\nSTDERR:\n{proc.stderr}"
        )
    return proc.stdout


def query_bestmove(engine: Path, variant: str, moves: list[str]) -> str:
    out = run_uci(
        engine,
        [
            "uci",
            "setoption name Threads value 1",
            f"setoption name UCI_Variant value {variant}",
            "position startpos" + ("" if not moves else " moves " + " ".join(moves)),
            "go depth 1",
        ],
    )
    for line in out.splitlines():
        m = MOVE_RE.match(line.strip())
        if m:
            return m.group(1)
    raise RuntimeError(f"missing bestmove for {variant}\n{out}")


def query_fen(engine: Path, variant: str, moves: list[str]) -> str:
    out = run_uci(
        engine,
        [
            "uci",
            f"setoption name UCI_Variant value {variant}",
            "position startpos" + ("" if not moves else " moves " + " ".join(moves)),
            "d",
        ],
    )
    for line in out.splitlines():
        m = FEN_RE.match(line.strip())
        if m:
            return m.group(1)
    raise RuntimeError(f"missing Fen line for {variant}\n{out}")


def query_move_count(engine: Path, variant: str, fen: str) -> int:
    out = run_uci(
        engine,
        [
            "uci",
            f"setoption name UCI_Variant value {variant}",
            f"position fen {fen}",
            "go perft 1",
        ],
    )
    for line in out.splitlines():
        m = PERFT_RE.match(line.strip())
        if m:
            return int(m.group(1))
    raise RuntimeError(f"missing perft node count for {variant}\n{out}")


def generate_baseline(upstream_engine: Path) -> dict:
    records = []
    for spec in SPECS:
        moves: list[str] = []
        for _ in range(spec.plies):
            bestmove = query_bestmove(upstream_engine, spec.variant, moves)
            if bestmove == "(none)":
                break
            moves.append(bestmove)
        fen = query_fen(upstream_engine, spec.variant, moves)
        move_count = query_move_count(upstream_engine, spec.variant, fen)
        records.append(
            {
                "name": spec.name,
                "variant": spec.variant,
                "plies": len(moves),
                "moves": moves,
                "fen": fen,
                "move_count": move_count,
            }
        )
    return {"source": str(upstream_engine), "records": records}


def verify(local_engine: Path, fixture_path: Path) -> int:
    fixture = json.loads(fixture_path.read_text())
    failed = False
    for record in fixture["records"]:
        actual = query_move_count(local_engine, record["variant"], record["fen"])
        expected = record["move_count"]
        if actual != expected:
            failed = True
            print(
                f"[FAIL] {record['name']} variant={record['variant']} expected={expected} actual={actual} fen={record['fen']}"
            )
        else:
            print(f"[OK] {record['name']} ({actual} moves)")
    return 1 if failed else 0


def main() -> int:
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser()
    parser.add_argument("local_engine", nargs="?", default=str(root / "src" / "stockfish"))
    parser.add_argument(
        "upstream_engine",
        nargs="?",
        default="/home/chris/fairy-stockfish-upstream/src/stockfish",
    )
    parser.add_argument(
        "--fixture",
        default=str(root / "tests" / "pgn" / "upstream_movecount_baseline.json"),
    )
    parser.add_argument("--regenerate", action="store_true")
    args = parser.parse_args()

    local_engine = Path(args.local_engine)
    upstream_engine = Path(args.upstream_engine)
    fixture_path = Path(args.fixture)

    if args.regenerate:
        if not upstream_engine.exists():
            print(f"upstream engine not found: {upstream_engine}", file=sys.stderr)
            return 2
        fixture = generate_baseline(upstream_engine)
        fixture_path.write_text(json.dumps(fixture, indent=2) + "\n")
        print(f"wrote {fixture_path}")
        return 0

    if not local_engine.exists():
        print(f"local engine not found: {local_engine}", file=sys.stderr)
        return 2
    if not fixture_path.exists():
        print(f"fixture not found: {fixture_path}", file=sys.stderr)
        return 2
    return verify(local_engine, fixture_path)


if __name__ == "__main__":
    raise SystemExit(main())
