#!/usr/bin/env python3

import argparse
import re
import subprocess
import sys
from pathlib import Path


MOVE_RE = re.compile(r"^([^:\s]+):\s+(\d+)\s*$")
NODES_RE = re.compile(r"^Nodes searched:\s+(\d+)\s*$")


def normalize_move(move: str) -> str:
    if move == "0000" or (len(move) == 4 and move[:2] == move[2:]):
        return "PASS"
    return move


def run_perft_divide(engine: Path, variant: str, position_cmd: str, depth: int) -> dict[str, int]:
    script = "\n".join(
        [
            "uci",
            "setoption name Threads value 1",
            f"setoption name UCI_Variant value {variant}",
            position_cmd,
            f"go perft {depth}",
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
        timeout=180,
    )
    if proc.returncode != 0:
        raise RuntimeError(
            f"{engine} failed with code {proc.returncode}\nSTDOUT:\n{proc.stdout}\nSTDERR:\n{proc.stderr}"
        )

    out: dict[str, int] = {}
    total_nodes = None
    for line in proc.stdout.splitlines():
        n = NODES_RE.match(line.strip())
        if n:
            total_nodes = int(n.group(1))
        m = MOVE_RE.match(line.strip())
        if not m:
            continue
        move = normalize_move(m.group(1))
        out[move] = int(m.group(2))
    if not out and total_nodes == 0:
        return {}
    if not out:
        raise RuntimeError(f"No perft divide output from {engine}\n{proc.stdout}")
    return out


def extend_position(position_cmd: str, move: str) -> str:
    encoded = "0000" if move == "PASS" else move
    if " moves " in position_cmd:
        return f"{position_cmd} {encoded}"
    return f"{position_cmd} moves {encoded}"


def first_divergence(
    local_engine: Path,
    upstream_engine: Path,
    variant: str,
    position_cmd: str,
    depth: int,
    line: list[str],
) -> tuple[list[str], dict[str, int], dict[str, int]] | None:
    if depth <= 0:
        return None

    local = run_perft_divide(local_engine, variant, position_cmd, depth)
    upstream = run_perft_divide(upstream_engine, variant, position_cmd, depth)
    if local == upstream:
        return None

    if depth == 1:
        return line, local, upstream

    shared_mismatch = sorted(
        (mv, local[mv] - upstream[mv])
        for mv in (set(local) & set(upstream))
        if local[mv] != upstream[mv]
    )
    shared_mismatch.sort(key=lambda x: abs(x[1]), reverse=True)

    if not shared_mismatch:
        return line, local, upstream

    for move, _ in shared_mismatch:
        child = first_divergence(
            local_engine,
            upstream_engine,
            variant,
            extend_position(position_cmd, move),
            depth - 1,
            line + [move],
        )
        if child is not None:
            return child

    return line, local, upstream


def main() -> int:
    parser = argparse.ArgumentParser(description="Trace first perft-divide divergence vs upstream.")
    parser.add_argument("local_engine")
    parser.add_argument("upstream_engine")
    parser.add_argument("--variant", required=True)
    parser.add_argument("--position", default="position startpos", help="Full UCI position command")
    parser.add_argument("--depth", type=int, required=True)
    args = parser.parse_args()

    local_engine = Path(args.local_engine)
    upstream_engine = Path(args.upstream_engine)

    if not local_engine.exists():
        print(f"local engine not found: {local_engine}", file=sys.stderr)
        return 2
    if not upstream_engine.exists():
        print(f"upstream engine not found: {upstream_engine}", file=sys.stderr)
        return 2

    result = first_divergence(
        local_engine,
        upstream_engine,
        args.variant,
        args.position,
        args.depth,
        [],
    )
    if result is None:
        print("No divergence found.")
        return 0

    path, local_div, upstream_div = result
    print(f"Divergence path ({len(path)} ply): {' '.join(path) if path else '<root>'}")
    print("Mismatched moves at this node:")
    for move in sorted(set(local_div) | set(upstream_div)):
        lv = local_div.get(move)
        uv = upstream_div.get(move)
        if lv != uv:
            print(f"  {move}: local={lv} upstream={uv}")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
