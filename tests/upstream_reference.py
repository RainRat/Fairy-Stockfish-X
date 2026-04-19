#!/usr/bin/env python3

import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


MOVE_RE = re.compile(r"^([^:\s]+):\s+(\d+)\s*$")


@dataclass(frozen=True)
class Case:
    name: str
    variant: str
    position_cmd: str


CASES = [
    Case("chess_startpos", "chess", "position startpos"),
    Case("berolina_startpos", "berolina", "position startpos"),
    Case("seirawan_startpos", "seirawan", "position startpos"),
    Case("torpedo_startpos", "torpedo", "position startpos"),
    Case("atomic_startpos", "atomic", "position startpos"),
    Case("xiangqi_startpos", "xiangqi", "position startpos"),
    Case("janggi_startpos", "janggi", "position startpos"),
    Case("checkers_startpos", "checkers", "position startpos"),
    Case(
        "janggi_cannon_selfcheck",
        "janggi",
        "position fen rnba1abnr/4k4/1c5c1/p1p1p1p1p/9/9/P1P1P1P1P/4C2C1/4K4/RNBA1ABNR b - - 0 1",
    ),
]


def run_perft(engine: Path, case: Case) -> dict[str, int]:
    script = "\n".join(
        [
            "uci",
            f"setoption name UCI_Variant value {case.variant}",
            case.position_cmd,
            "go perft 1",
            "quit",
            "",
        ]
    )
    proc = subprocess.run(
        [str(engine)],
        input=script,
        text=True,
        capture_output=True,
        check=False,
        timeout=60,
    )
    if proc.returncode != 0:
        raise RuntimeError(
            f"{engine} failed for {case.name} with code {proc.returncode}\nSTDOUT:\n{proc.stdout}\nSTDERR:\n{proc.stderr}"
        )

    moves: dict[str, int] = {}
    for line in proc.stdout.splitlines():
        m = MOVE_RE.match(line)
        if m:
            move = m.group(1)
            if move == "0000" or (len(move) == 4 and move[:2] == move[2:]):
                move = "PASS"
            moves[move] = int(m.group(2))

    if not moves:
        raise RuntimeError(f"{engine} produced no perft moves for {case.name}\n{proc.stdout}")

    return moves


def main() -> int:
    root = Path(__file__).resolve().parent.parent
    local_engine = Path(sys.argv[1]) if len(sys.argv) > 1 else root / "src" / "stockfish"
    upstream_engine = (
        Path(sys.argv[2])
        if len(sys.argv) > 2
        else Path("/home/chris/fairy-stockfish-upstream/src/stockfish")
    )

    if not local_engine.exists():
        print(f"local engine not found: {local_engine}", file=sys.stderr)
        return 2
    if not upstream_engine.exists():
        print(f"upstream engine not found: {upstream_engine}", file=sys.stderr)
        return 2

    failed = False
    for case in CASES:
        local_moves = run_perft(local_engine, case)
        upstream_moves = run_perft(upstream_engine, case)
        if local_moves != upstream_moves:
            failed = True
            local_only = sorted(set(local_moves) - set(upstream_moves))
            upstream_only = sorted(set(upstream_moves) - set(local_moves))
            shared_mismatch = sorted(
                move
                for move in (set(local_moves) & set(upstream_moves))
                if local_moves[move] != upstream_moves[move]
            )
            print(f"[FAIL] {case.name}")
            if local_only:
                print(f"  local only: {local_only}")
            if upstream_only:
                print(f"  upstream only: {upstream_only}")
            if shared_mismatch:
                print(
                    "  count mismatch: "
                    + ", ".join(
                        f"{mv} local={local_moves[mv]} upstream={upstream_moves[mv]}"
                        for mv in shared_mismatch
                    )
                )
        else:
            print(f"[OK] {case.name} ({len(local_moves)} moves)")

    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
