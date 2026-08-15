#!/usr/bin/env python3
"""Stress-test FSX Arimaa move and terminal-state parity.

This deliberately chooses turns with the independent Python referee, then
audits randomized reachable boundary positions with FSX ``go perft 1``.
The reference and engine therefore do not share move-generation code.
"""

from __future__ import annotations

import argparse
import collections
import random
import sys
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS))

from arimaa_tournament import (  # noqa: E402
    ArimaaState,
    ProcessAdapter,
    TournamentError,
    TurnCursor,
)


class StressError(RuntimeError):
    pass


class FSXPerftAdapter(ProcessAdapter):
    def __init__(self, command: str, variant_path: str, timeout: float):
        super().__init__(command, "FSX perft", timeout)
        self.variant_path = variant_path

    def start(self) -> None:
        super().start()
        self.send("uci")
        self.wait_for("uciok")
        self.send(f"setoption name VariantPath value {self.variant_path}")
        self.send("setoption name UCI_Variant value arimaa")
        self.send("setoption name Threads value 1")
        self.send("setoption name Verbosity value 0")
        self.send("isready")
        self.wait_for("readyok")

    def perft(self, state: ArimaaState) -> tuple[int, collections.Counter[str]]:
        self.send(f"position fen {state.fen()}")
        self.send("go perft 1")

        root_moves: collections.Counter[str] = collections.Counter()
        while True:
            line = self.protocol_line(self.read_line())
            if line.startswith("Nodes searched:"):
                return int(line.split(":", 1)[1].strip()), root_moves
            if line.endswith(": 1"):
                root_moves[line[:-3]] += 1


def reference_turns(state: ArimaaState) -> list[str]:
    """Return legal complete-turn notation from the independent referee."""

    start_board = dict(state.board.board)
    result: list[str] = []

    def walk(cursor: TurnCursor) -> None:
        for action in cursor.board.candidate_actions():
            if cursor.physical_steps + len(action.steps) > 4:
                continue

            child = cursor.clone()
            try:
                child.add_steps(list(action.steps))
                child_state = state.finish_turn(
                    child,
                    adjudicate_no_move=False,
                    allow_unchanged=True,
                    enforce_repetition=False,
                    adjudicate_terminal=False,
                )
            except TournamentError:
                continue

            boundary_legal = child_state.repetitions[child_state.board.key()] < 3
            if boundary_legal and child.board.board != start_board:
                result.append(",".join(item.fsx() for item in child.actions))
            if child.physical_steps < 4:
                walk(child)

    walk(TurnCursor(state.board.copy()))
    return result


def random_turn(state: ArimaaState, rng: random.Random):
    """Choose and apply one legal turn without using FSX move generation."""

    start_board = dict(state.board.board)
    cursor = TurnCursor(state.board.copy())

    while True:
        choices = []
        for action in cursor.board.candidate_actions():
            if cursor.physical_steps + len(action.steps) > 4:
                continue
            child = cursor.clone()
            try:
                child.add_steps(list(action.steps))
                child_state = state.finish_turn(
                    child,
                    adjudicate_no_move=False,
                    allow_unchanged=True,
                    enforce_repetition=False,
                    adjudicate_terminal=False,
                )
            except TournamentError:
                continue
            boundary_legal = child_state.repetitions[child_state.board.key()] < 3
            if boundary_legal and child.board.board != start_board:
                choices.append((child, True))
            elif child.physical_steps < 4:
                choices.append((child, False))

        if not choices:
            raise StressError(f"referee found no legal turn at {state.fen()}")

        cursor, can_end = rng.choice(choices)
        if can_end and (cursor.physical_steps == 4 or rng.random() < 0.25):
            return state.finish_turn(cursor)
        if cursor.physical_steps == 4:
            return state.finish_turn(cursor)


def audit_position(adapter: FSXPerftAdapter,
                   state: ArimaaState,
                   label: str,
                   exact: bool) -> None:
    reference = reference_turns(state) if exact else None
    expected = len(reference) if reference is not None else state.legal_turn_count()
    actual, engine_moves = adapter.perft(state)
    if actual != expected:
        raise StressError(
            f"{label}: perft mismatch at {state.fen()}: "
            f"FSX={actual}, referee={expected}"
        )

    if reference is not None:
        expected_moves = collections.Counter(reference)
        if engine_moves != expected_moves:
            missing = list((expected_moves - engine_moves).elements())[:8]
            extra = list((engine_moves - expected_moves).elements())[:8]
            raise StressError(
                f"{label}: root move-set mismatch at {state.fen()}; "
                f"missing={missing}, extra={extra}"
            )

    print(f"ok {label}: {actual} legal turns")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fsx", required=True)
    parser.add_argument("--variant-path", default="src/variants.ini")
    parser.add_argument("--seed", type=int, default=20260814)
    parser.add_argument("--games", type=int, default=4)
    parser.add_argument("--max-turns", type=int, default=120)
    parser.add_argument("--samples-per-game", type=int, default=8)
    parser.add_argument("--exact-samples", type=int, default=8,
                        help="number of sampled positions to compare by exact root move set")
    parser.add_argument("--timeout", type=float, default=120.0)
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    if min(args.games, args.max_turns, args.samples_per_game) < 1:
        raise SystemExit("games, max-turns, and samples-per-game must be positive")

    rng = random.Random(args.seed)
    adapter = FSXPerftAdapter(args.fsx, args.variant_path, args.timeout)
    sampled = 0
    terminal = 0
    try:
        adapter.start()
        opening = ArimaaState.standard_setup()
        audit_position(adapter, opening, "opening", exact=True)

        for game in range(1, args.games + 1):
            state = ArimaaState.standard_setup()
            sample_turns = set(rng.sample(
                range(1, args.max_turns + 1), args.samples_per_game
            ))
            for turn in range(1, args.max_turns + 1):
                if turn in sample_turns and state.winner is None:
                    audit_position(
                        adapter,
                        state,
                        f"game {game} turn {turn}",
                        exact=sampled < args.exact_samples,
                    )
                    sampled += 1

                if state.winner is not None:
                    break
                state = random_turn(state, rng)

            if state.winner is not None:
                terminal += 1
                actual, _ = adapter.perft(state)
                if actual != 0:
                    raise StressError(
                        f"game {game}: terminal {state.reason} position still has "
                        f"{actual} FSX turns at {state.fen()}"
                    )
                print(f"ok game {game}: terminal {state.reason}, winner={state.winner}")

        print(
            f"parity stress passed: seed={args.seed}, games={args.games}, "
            f"samples={sampled}, terminal-games={terminal}"
        )
        return 0
    finally:
        adapter.close()


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (StressError, TournamentError) as error:
        print(f"parity stress error: {error}", file=sys.stderr)
        raise SystemExit(2)
