#!/usr/bin/env python3

import argparse
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Optional


MOVE_RE = re.compile(r"^([^:\s]+):\s+(\d+)\s*$")
VARIANT_RE = re.compile(r"^option name UCI_Variant type combo default \S+ var (.+)$")


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
    Case("allexplodeatomic_startpos", "allexplodeatomic", "position startpos"),
    Case("duck_startpos", "duck", "position startpos"),
    Case("spartan_startpos", "spartan", "position startpos"),
    Case("racingkings_startpos", "racingkings", "position startpos"),
    Case("xiangqi_startpos", "xiangqi", "position startpos"),
    Case("janggi_startpos", "janggi", "position startpos"),
]


def run_perft(
    engine: Path, case: Case, variant_path: Optional[Path] = None
) -> dict[str, int]:
    setup = [] if variant_path is None else [f"setoption name VariantPath value {variant_path}"]
    script = "\n".join(
        setup
        + [
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


def available_variants(engine: Path, variant_path: Optional[Path] = None) -> set[str]:
    setup = [] if variant_path is None else [f"setoption name VariantPath value {variant_path}"]
    proc = subprocess.run(
        [str(engine)],
        input="\n".join(setup + ["uci", "quit", ""]),
        text=True,
        capture_output=True,
        check=False,
        timeout=30,
    )
    if proc.returncode != 0:
        raise RuntimeError(
            f"{engine} failed while probing variants with code {proc.returncode}\nSTDOUT:\n{proc.stdout}\nSTDERR:\n{proc.stderr}"
        )
    variants = set()
    for line in proc.stdout.splitlines():
        m = VARIANT_RE.match(line.strip())
        if not m:
            continue
        variants.update(m.group(1).split())
    return variants


def allowed_missing_variants(values: list[str], smoke_only: bool) -> set[str]:
    known = {case.variant for case in CASES}
    unknown = set(values) - known
    if unknown:
        raise ValueError(
            "unknown variant in missing-variant allowlist: "
            + ", ".join(sorted(unknown))
        )
    return known if smoke_only else set(values)


def main() -> int:
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser()
    parser.add_argument("local_engine", nargs="?", default=str(root / "src" / "stockfish"))
    parser.add_argument("upstream_engine", nargs="?")
    parser.add_argument(
        "--allow-missing-variant",
        action="append",
        default=[],
        metavar="NAME",
        help="allow NAME to be absent from either engine (repeatable)",
    )
    parser.add_argument(
        "--smoke-only",
        action="store_true",
        help="allow declared comparison variants to be absent, but require one comparison",
    )
    args = parser.parse_args()
    if args.upstream_engine is None:
        parser.error("UPSTREAM_ENGINE is required")

    try:
        allowed_missing = allowed_missing_variants(
            args.allow_missing_variant, args.smoke_only
        )
    except ValueError as error:
        parser.error(str(error))

    local_engine = Path(args.local_engine)
    upstream_engine = Path(args.upstream_engine)

    if not local_engine.exists():
        print(f"local engine not found: {local_engine}", file=sys.stderr)
        return 2
    if not upstream_engine.exists():
        print(f"upstream engine not found: {upstream_engine}", file=sys.stderr)
        return 2

    failed = False
    compared = 0
    local_variant_path = root / "src" / "variants.ini"
    local_variants = available_variants(local_engine, local_variant_path)
    upstream_variants = available_variants(upstream_engine)
    for case in CASES:
        missing = []
        if case.variant not in local_variants:
            missing.append("local")
        if case.variant not in upstream_variants:
            missing.append("upstream")
        if missing:
            if case.variant in allowed_missing:
                print(
                    f"[SKIP] {case.name} variant={case.variant} "
                    f"missing from {', '.join(missing)} (allowlisted)"
                )
            else:
                failed = True
                print(
                    f"[FAIL] {case.name} variant={case.variant} "
                    f"missing from {', '.join(missing)}"
                )
            continue
        compared += 1
        local_moves = run_perft(local_engine, case, local_variant_path)
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

    if compared == 0:
        failed = True
        print("[FAIL] no comparable upstream-reference cases ran")

    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
