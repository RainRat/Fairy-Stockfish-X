#!/usr/bin/env python3
"""Run complete-turn Arimaa games between an AEI bot and FSX.

The runner deliberately keeps a small, independent rules referee.  Engines
only propose turns; the runner translates their notation, applies the turn
atomically where required, enforces the physical four-step limit, and owns
terminal adjudication.  The initial tournament profile uses a fixed completed
setup so setup placement is not mixed into protocol testing.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import selectors
import shlex
import subprocess
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable, Optional


FILES = "abcdefgh"
PIECE_TYPES = "rcdhme"
STRENGTH = {piece: index for index, piece in enumerate(PIECE_TYPES, start=1)}
TRAPS = frozenset(("c3", "f3", "c6", "f6"))

# A deterministic, completed-turn opening position.  It is the same profile
# used by the focused FSX Arimaa tests; real setup placement can be added as a
# separate protocol experiment later.
STANDARD_SETUP_FEN = "cdhmehdc/rrrrrrrr/8/8/8/8/RRRRRRRR/CDHMEHDC w - - 0 1"

AEI_STEP_RE = re.compile(r"^([RCDHMErcdhme])([a-h][1-8])([nesw])$")
AEI_TRAP_RE = re.compile(r"^([RCDHMErcdhme])([a-h][1-8])x$")
COORD_RE = re.compile(r"^[a-h][1-8]$")
FSX_COORD_RE = re.compile(r"^([a-h][1-8])([a-h][1-8])$")


class TournamentError(RuntimeError):
    """A protocol, notation, or adjudication failure."""


def side_of(piece: str) -> str:
    if not piece or piece.lower() not in PIECE_TYPES:
        raise TournamentError(f"invalid Arimaa piece: {piece!r}")
    return "g" if piece.isupper() else "s"


def piece_type(piece: str) -> str:
    return piece.lower()


def piece_for(side: str, kind: str) -> str:
    return kind.upper() if side == "g" else kind


def opposite(side: str) -> str:
    return "s" if side == "g" else "g"


def square(file_index: int, rank: int) -> str:
    if not (0 <= file_index < 8 and 1 <= rank <= 8):
        raise TournamentError(f"off-board square: {file_index},{rank}")
    return f"{FILES[file_index]}{rank}"


def square_parts(value: str) -> tuple[int, int]:
    if not COORD_RE.fullmatch(value):
        raise TournamentError(f"invalid square: {value!r}")
    return FILES.index(value[0]), int(value[1])


def adjacent(a: str, b: str) -> bool:
    af, ar = square_parts(a)
    bf, br = square_parts(b)
    return abs(af - bf) + abs(ar - br) == 1


def step_direction(src: str, dst: str) -> str:
    sf, sr = square_parts(src)
    df, dr = square_parts(dst)
    if abs(sf - df) + abs(sr - dr) != 1:
        raise TournamentError(f"not an orthogonal step: {src}{dst}")
    if df > sf:
        return "e"
    if df < sf:
        return "w"
    if dr > sr:
        return "n"
    return "s"


def step_destination(src: str, direction: str) -> str:
    file_index, rank = square_parts(src)
    if direction == "n":
        rank += 1
    elif direction == "s":
        rank -= 1
    elif direction == "e":
        file_index += 1
    elif direction == "w":
        file_index -= 1
    else:
        raise TournamentError(f"invalid AEI direction: {direction!r}")
    return square(file_index, rank)


@dataclass(frozen=True)
class PhysicalStep:
    piece: str
    src: str
    dst: str

    @classmethod
    def from_aei(cls, token: str) -> "PhysicalStep":
        match = AEI_STEP_RE.fullmatch(token)
        if not match:
            raise TournamentError(f"invalid AEI step: {token!r}")
        piece, src, direction = match.groups()
        return cls(piece, src, step_destination(src, direction))

    @classmethod
    def from_coordinates(cls, board: "ArimaaBoard", src: str, dst: str) -> "PhysicalStep":
        piece = board.board.get(src)
        if piece is None:
            raise TournamentError(f"no piece on {src}")
        step_direction(src, dst)
        return cls(piece, src, dst)

    def aei(self) -> str:
        return f"{self.piece}{self.src}{step_direction(self.src, self.dst)}"


@dataclass(frozen=True)
class Action:
    kind: str
    steps: tuple[PhysicalStep, ...]

    def aei(self) -> str:
        return " ".join(step.aei() for step in self.steps)

    def fsx(self) -> str:
        if self.kind == "normal":
            step = self.steps[0]
            return step.src + step.dst
        if self.kind == "push":
            # Official push order is enemy first, then the stronger piece.
            enemy, pusher = self.steps
            return f"{pusher.src}{pusher.dst},{enemy.dst}"
        if self.kind == "pull":
            pusher, enemy = self.steps
            return f"{pusher.src}{pusher.dst},{enemy.src}"
        raise TournamentError(f"unknown action kind: {self.kind}")


@dataclass
class TurnCursor:
    """A mutable position while one side is constructing a complete turn."""

    board: "ArimaaBoard"
    actions: list[Action] = field(default_factory=list)
    physical_steps: int = 0

    def clone(self) -> "TurnCursor":
        return TurnCursor(self.board.copy(), list(self.actions), self.physical_steps)

    def add_steps(self, steps: list[PhysicalStep]) -> None:
        if self.physical_steps + len(steps) > 4:
            raise TournamentError("Arimaa turn exceeds four physical steps")

        index = 0
        while index < len(steps):
            current = steps[index]
            if side_of(current.piece) == self.board.side:
                if index + 1 < len(steps):
                    following = steps[index + 1]
                    if self.board.valid_pull(current, following):
                        self.board.apply_pull(current, following)
                        self.actions.append(Action("pull", (current, following)))
                        index += 2
                        continue

                self.board.apply_normal(current)
                self.actions.append(Action("normal", (current,)))
                index += 1
                continue

            if index + 1 >= len(steps):
                raise TournamentError("pushed piece is not followed by its pusher")
            following = steps[index + 1]
            if not self.board.valid_push(current, following):
                raise TournamentError(
                    f"illegal enemy step {current.aei()} without a completing push"
                )
            self.board.apply_push(current, following)
            self.actions.append(Action("push", (current, following)))
            index += 2

        self.physical_steps += len(steps)


@dataclass
class TurnResult:
    state: "ArimaaState"
    actions: list[Action]
    physical_steps: list[PhysicalStep]

    def aei(self) -> str:
        return " ".join(step.aei() for step in self.physical_steps)

    def fsx(self) -> str:
        return ",".join(action.fsx() for action in self.actions)


@dataclass
class ArimaaBoard:
    board: dict[str, str]
    side: str = "g"

    def copy(self) -> "ArimaaBoard":
        return ArimaaBoard(dict(self.board), self.side)

    @classmethod
    def from_fen(cls, fen: str) -> "ArimaaBoard":
        fields = fen.split()
        if len(fields) < 2:
            raise TournamentError(f"invalid FEN: {fen!r}")
        placement, active = fields[0], fields[1]
        side = {"w": "g", "b": "s", "g": "g", "s": "s"}.get(active)
        if side is None:
            raise TournamentError(f"invalid Arimaa side: {active!r}")

        ranks = placement.split("/")
        if len(ranks) != 8:
            raise TournamentError(f"invalid Arimaa board ranks: {placement!r}")
        pieces: dict[str, str] = {}
        for rank_offset, row in enumerate(ranks):
            rank = 8 - rank_offset
            file_index = 0
            for token in row:
                if token.isdigit():
                    file_index += int(token)
                    continue
                if token not in "RCDHMErcdhme":
                    raise TournamentError(f"invalid Arimaa board piece: {token!r}")
                if file_index >= 8:
                    raise TournamentError(f"too many files in FEN rank: {row!r}")
                pieces[square(file_index, rank)] = token
                file_index += 1
            if file_index != 8:
                raise TournamentError(f"short FEN rank: {row!r}")
        result = cls(pieces, side)
        result.stabilize_traps()
        return result

    def placement(self) -> str:
        rows: list[str] = []
        for rank in range(8, 0, -1):
            row = ""
            empty = 0
            for file_index in range(8):
                piece = self.board.get(square(file_index, rank))
                if piece is None:
                    empty += 1
                else:
                    if empty:
                        row += str(empty)
                        empty = 0
                    row += piece
            if empty:
                row += str(empty)
            rows.append(row)
        return "/".join(rows)

    def fen(self, fullmove: int = 1) -> str:
        active = "w" if self.side == "g" else "b"
        return f"{self.placement()} {active} - - 0 {fullmove}"

    def aei_board(self) -> str:
        return "".join(self.board.get(square(file_index, rank), " ")
                       for rank in range(8, 0, -1)
                       for file_index in range(8))

    def key(self) -> tuple[str, str]:
        return self.placement(), self.side

    def friendly_adjacent(self, value: str, side: str) -> bool:
        file_index, rank = square_parts(value)
        for df, dr in ((0, 1), (1, 0), (0, -1), (-1, 0)):
            if 0 <= file_index + df < 8 and 1 <= rank + dr <= 8:
                piece = self.board.get(square(file_index + df, rank + dr))
                if piece is not None and side_of(piece) == side:
                    return True
        return False

    def adjacent_squares(self, value: str) -> Iterable[str]:
        file_index, rank = square_parts(value)
        for df, dr in ((0, 1), (1, 0), (0, -1), (-1, 0)):
            if 0 <= file_index + df < 8 and 1 <= rank + dr <= 8:
                yield square(file_index + df, rank + dr)

    def is_frozen(self, value: str) -> bool:
        piece = self.board.get(value)
        if piece is None:
            return False
        side = side_of(piece)
        if self.friendly_adjacent(value, side):
            return False
        strength = STRENGTH[piece_type(piece)]
        return any(
            (neighbor := self.board.get(adjacent_square)) is not None
            and side_of(neighbor) != side
            and STRENGTH[piece_type(neighbor)] > strength
            for adjacent_square in self.adjacent_squares(value)
        )

    def own_step_allowed(self, step: PhysicalStep, board: Optional["ArimaaBoard"] = None) -> bool:
        view = board or self
        piece = view.board.get(step.src)
        if piece != step.piece or side_of(piece) != view.side:
            return False
        if view.board.get(step.dst) is not None or not adjacent(step.src, step.dst):
            return False
        if view.is_frozen(step.src):
            return False
        if piece_type(piece) == "r":
            _, src_rank = square_parts(step.src)
            _, dst_rank = square_parts(step.dst)
            if view.side == "g" and dst_rank < src_rank:
                return False
            if view.side == "s" and dst_rank > src_rank:
                return False
        return True

    def valid_pull(self, first: PhysicalStep, second: PhysicalStep) -> bool:
        if side_of(first.piece) != self.side or side_of(second.piece) == self.side:
            return False
        if not self.own_step_allowed(first):
            return False
        if second.src == first.src or second.dst != first.src:
            return False
        pulled = self.board.get(second.src)
        if pulled != second.piece or side_of(pulled) == self.side:
            return False
        if not adjacent(first.src, second.src):
            return False
        return STRENGTH[piece_type(first.piece)] > STRENGTH[piece_type(pulled)]

    def valid_push(self, first: PhysicalStep, second: PhysicalStep) -> bool:
        if side_of(first.piece) == self.side or side_of(second.piece) != self.side:
            return False
        pushed = self.board.get(first.src)
        pusher = self.board.get(second.src)
        if pushed != first.piece or pusher != second.piece:
            return False
        if self.board.get(first.dst) is not None:
            return False
        if second.dst != first.src or not adjacent(first.src, first.dst):
            return False
        if not adjacent(second.src, first.src):
            return False
        if STRENGTH[piece_type(pusher)] <= STRENGTH[piece_type(pushed)]:
            return False

        # The enemy step happens first.  Freeze and movement legality of the
        # pusher are therefore checked against that intermediate board, while
        # trap removal waits until both steps are applied atomically.
        intermediate = self.copy()
        del intermediate.board[first.src]
        intermediate.board[first.dst] = first.piece
        return intermediate.own_step_allowed(second)

    def apply_normal(self, step: PhysicalStep) -> None:
        if not self.own_step_allowed(step):
            raise TournamentError(f"illegal normal step: {step.aei()}")
        del self.board[step.src]
        self.board[step.dst] = step.piece
        self.stabilize_traps()

    def apply_pull(self, first: PhysicalStep, second: PhysicalStep) -> None:
        if not self.valid_pull(first, second):
            raise TournamentError(f"illegal pull: {first.aei()} {second.aei()}")
        del self.board[first.src]
        del self.board[second.src]
        self.board[first.dst] = first.piece
        self.board[second.dst] = second.piece
        self.stabilize_traps()

    def apply_push(self, first: PhysicalStep, second: PhysicalStep) -> None:
        if not self.valid_push(first, second):
            raise TournamentError(f"illegal push: {first.aei()} {second.aei()}")
        del self.board[first.src]
        del self.board[second.src]
        self.board[first.dst] = first.piece
        self.board[second.dst] = second.piece
        self.stabilize_traps()

    def stabilize_traps(self) -> None:
        # Removing one trap occupant can expose another unsupported occupant;
        # repeat until the board is stable.
        while True:
            removed = False
            for trap in TRAPS:
                piece = self.board.get(trap)
                if piece is not None and not self.friendly_adjacent(trap, side_of(piece)):
                    del self.board[trap]
                    removed = True
            if not removed:
                return

    def candidate_actions(self) -> Iterable[Action]:
        """Yield complete first actions useful for no-move adjudication."""
        own = [square(file_index, rank)
               for rank in range(1, 9)
               for file_index in range(8)
               if (piece := self.board.get(square(file_index, rank))) is not None
               and side_of(piece) == self.side]
        for src in own:
            for dst in self.adjacent_squares(src):
                piece = self.board[src]
                step = PhysicalStep(piece, src, dst)
                if self.own_step_allowed(step):
                    yield Action("normal", (step,))
                    for enemy_src in self.adjacent_squares(src):
                        enemy_piece = self.board.get(enemy_src)
                        if enemy_piece is None or side_of(enemy_piece) == self.side:
                            continue
                        pulled = PhysicalStep(enemy_piece, enemy_src, src)
                        if self.valid_pull(step, pulled):
                            yield Action("pull", (step, pulled))

        enemy = [square(file_index, rank)
                 for rank in range(1, 9)
                 for file_index in range(8)
                 if (piece := self.board.get(square(file_index, rank))) is not None
                 and side_of(piece) != self.side]
        for enemy_src in enemy:
            enemy_piece = self.board[enemy_src]
            for enemy_dst in self.adjacent_squares(enemy_src):
                if self.board.get(enemy_dst) is not None:
                    continue
                first = PhysicalStep(enemy_piece, enemy_src, enemy_dst)
                for pusher_src in self.adjacent_squares(enemy_src):
                    pusher_piece = self.board.get(pusher_src)
                    if pusher_piece is None or side_of(pusher_piece) != self.side:
                        continue
                    second = PhysicalStep(pusher_piece, pusher_src, enemy_src)
                    if self.valid_push(first, second):
                        yield Action("push", (first, second))


@dataclass
class ArimaaState:
    board: ArimaaBoard
    fullmove: int = 1
    repetitions: dict[tuple[str, str], int] = field(default_factory=dict)
    winner: Optional[str] = None
    reason: Optional[str] = None
    history: tuple[str, ...] = ()
    start_fen: str = ""

    @classmethod
    def from_fen(cls, fen: str) -> "ArimaaState":
        board = ArimaaBoard.from_fen(fen)
        fullmove = int(fen.split()[5]) if len(fen.split()) > 5 else 1
        state = cls(board, fullmove=fullmove, start_fen=board.fen(fullmove))
        state.repetitions[board.key()] = 1
        return state

    @classmethod
    def standard_setup(cls) -> "ArimaaState":
        return cls.from_fen(STANDARD_SETUP_FEN)

    def copy(self) -> "ArimaaState":
        return ArimaaState(
            self.board.copy(),
            self.fullmove,
            dict(self.repetitions),
            self.winner,
            self.reason,
            self.history,
            self.start_fen,
        )

    def fen(self) -> str:
        return self.board.fen(self.fullmove)

    def aei_position(self) -> str:
        return f"{self.board.side} [{self.board.aei_board()}]"

    def terminal_winner(self, mover: str) -> tuple[Optional[str], Optional[str]]:
        gold_goal = any(piece == "R" and square_parts(sq)[1] == 8
                        for sq, piece in self.board.board.items())
        silver_goal = any(piece == "r" and square_parts(sq)[1] == 1
                          for sq, piece in self.board.board.items())
        if (mover == "g" and gold_goal) or (mover == "s" and silver_goal):
            return mover, "goal"
        if (mover == "g" and silver_goal) or (mover == "s" and gold_goal):
            return opposite(mover), "goal"

        gold_rabbits = any(piece == "R" for piece in self.board.board.values())
        silver_rabbits = any(piece == "r" for piece in self.board.board.values())
        if not gold_rabbits and not silver_rabbits:
            return mover, "both-rabbit-loss"
        if not gold_rabbits:
            return "s", "rabbit-loss"
        if not silver_rabbits:
            return "g", "rabbit-loss"
        return None, None

    def finish_turn(self, cursor: TurnCursor, adjudicate_no_move: bool = True,
                    allow_unchanged: bool = False,
                    enforce_repetition: bool = True,
                    adjudicate_terminal: bool = True) -> "ArimaaState":
        if cursor.physical_steps < 1 or cursor.physical_steps > 4:
            raise TournamentError("Arimaa turn must contain one to four physical steps")
        if cursor.board.board == self.board.board and not allow_unchanged:
            raise TournamentError("whole-turn pass is illegal")

        next_board = cursor.board.copy()
        mover = self.board.side
        next_board.side = opposite(mover)
        next_fullmove = self.fullmove + (1 if mover == "s" else 0)
        key = next_board.key()
        repetitions = dict(self.repetitions)
        repetitions[key] = repetitions.get(key, 0) + 1
        if enforce_repetition and repetitions[key] >= 3:
            raise TournamentError("third occurrence of a position is illegal")

        turn_text = ",".join(action.fsx() for action in cursor.actions)
        result = ArimaaState(next_board, next_fullmove, repetitions,
                             history=self.history + (turn_text,),
                             start_fen=self.start_fen)
        if adjudicate_terminal:
            winner, reason = result.terminal_winner(mover)
            if winner is None and adjudicate_no_move and not result.has_legal_turn():
                winner, reason = mover, "no-legal-move"
            result.winner = winner
            result.reason = reason
        return result

    def apply_turn(self, steps: list[PhysicalStep]) -> TurnResult:
        if self.winner is not None:
            raise TournamentError("game is already over")
        if not steps:
            raise TournamentError("empty Arimaa turn is illegal")
        if len(steps) > 4:
            raise TournamentError("Arimaa turn exceeds four physical steps")
        cursor = TurnCursor(self.board.copy())
        cursor.add_steps(steps)
        result = self.finish_turn(cursor)
        return TurnResult(result, cursor.actions, steps)

    def has_legal_turn(self) -> bool:
        if self.winner is not None:
            return False
        start_board = dict(self.board.board)

        def walk(cursor: TurnCursor) -> bool:
            for action in cursor.board.candidate_actions():
                if cursor.physical_steps + len(action.steps) > 4:
                    continue
                child = cursor.clone()
                child.add_steps(list(action.steps))
                try:
                    child_state = self.finish_turn(
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
                    return True
                if child.physical_steps < 4 and walk(child):
                    return True
            return False

        return walk(TurnCursor(self.board.copy()))

    def legal_turn_count(self) -> int:
        """Count legal complete turns from this boundary position.

        This is intended for referee audits and focused tests, not the engine
        hot path.  The tournament loop only needs the early-exit
        ``has_legal_turn`` check.
        """
        if self.winner is not None:
            return 0
        start_board = dict(self.board.board)

        def count(cursor: TurnCursor) -> int:
            total = 0
            for action in cursor.board.candidate_actions():
                if cursor.physical_steps + len(action.steps) > 4:
                    continue
                child = cursor.clone()
                child.add_steps(list(action.steps))
                try:
                    child_state = self.finish_turn(
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
                    total += 1
                if child.physical_steps < 4:
                    total += count(child)
            return total

        return count(TurnCursor(self.board.copy()))


def parse_aei_move(payload: str) -> list[PhysicalStep]:
    payload = payload.strip()
    if not payload or payload in {"pass", "0000", "(none)"}:
        raise TournamentError(f"empty/pass AEI move is illegal: {payload!r}")
    steps = []
    for token in payload.split():
        # Akimot annotates a step that leaves an unsupported trap occupant as
        # an additional `PieceSquarex` record.  Trap removal is already
        # applied by the independent referee after each physical step, so the
        # annotation is informational and consumes no step.
        if AEI_TRAP_RE.fullmatch(token):
            continue
        steps.append(PhysicalStep.from_aei(token))
    if not steps:
        raise TournamentError(f"AEI move contains no physical steps: {payload!r}")
    return steps


def _fsx_coordinate(board: ArimaaBoard, text: str) -> tuple[str, str, PhysicalStep]:
    match = FSX_COORD_RE.fullmatch(text)
    if not match:
        raise TournamentError(f"invalid FSX coordinate move: {text!r}")
    src, dst = match.groups()
    return src, dst, PhysicalStep.from_coordinates(board, src, dst)


def parse_fsx_move(state: ArimaaState, payload: str) -> list[PhysicalStep]:
    """Expand FSX coordinate/pull/push output into physical AEI steps."""
    payload = payload.strip()
    if payload in {"(none)", "0000", "pass", ""}:
        raise TournamentError(f"empty/pass FSX move is illegal: {payload!r}")

    # Reparse each prefix from the boundary so a compact pull is recognized as
    # atomic even when its comma is indistinguishable from a turn separator.
    physical: list[PhysicalStep] = []
    offset = 0
    while offset < len(payload):
        if payload[offset] in ",;":
            offset += 1
        if offset >= len(payload):
            raise TournamentError("FSX move ends after a separator")

        prefix = payload[offset:]
        candidates: list[tuple[int, list[PhysicalStep]]] = []

        # Pull notation is exactly "fromto,secondary".  Require a real token
        # boundary after the secondary square to avoid reading the first two
        # characters of the next ordinary coordinate move.
        if len(prefix) >= 7 and prefix[4] == ",":
            compound_text = prefix[:7]
            if len(prefix) == 7 or prefix[7] in ",;":
                try:
                    board = _cursor_from_physical(state, physical).board
                    src, dst, first = _fsx_coordinate(board, compound_text[:4])
                    secondary = compound_text[5:7]
                    pushed = board.board.get(dst)
                    if (side_of(first.piece) == board.side
                            and pushed is not None
                            and side_of(pushed) != board.side
                            and board.board.get(secondary) is None
                            and board.valid_push(PhysicalStep(pushed, dst, secondary), first)):
                        candidates.append((7, [PhysicalStep(pushed, dst, secondary), first]))

                    enemy = board.board.get(secondary)
                    if (side_of(first.piece) == board.side
                            and board.board.get(dst) is None
                            and enemy is not None
                            and side_of(enemy) != board.side
                            and board.valid_pull(first, PhysicalStep(enemy, secondary, src))):
                        candidates.append((7, [first, PhysicalStep(enemy, secondary, src)]))
                except TournamentError:
                    pass

        if len(prefix) >= 4:
            move_text = prefix[:4]
            if len(prefix) == 4 or prefix[4] in ",;":
                try:
                    board = _cursor_from_physical(state, physical).board
                    src, dst, first = _fsx_coordinate(board, move_text)
                    if side_of(first.piece) == board.side and board.board.get(dst) is None:
                        candidates.append((4, [first]))
                    elif side_of(first.piece) == board.side and board.board.get(dst) is not None:
                        # FSX's stepwise Arimaa push is encoded by the pusher's
                        # coordinate move into the occupied enemy square.
                        df = square_parts(dst)[0] - square_parts(src)[0]
                        dr = square_parts(dst)[1] - square_parts(src)[1]
                        beyond = square(square_parts(dst)[0] + (1 if df > 0 else -1 if df < 0 else 0),
                                       square_parts(dst)[1] + (1 if dr > 0 else -1 if dr < 0 else 0))
                        pushed = board.board.get(dst)
                        if pushed is not None and board.board.get(beyond) is None:
                            enemy_step = PhysicalStep(pushed, dst, beyond)
                            candidates.append((4, [enemy_step, first]))
                except TournamentError:
                    pass

        accepted = None
        for width, candidate in candidates:
            try:
                _cursor_from_physical(state, physical + candidate)
            except TournamentError:
                continue
            accepted = width, candidate
            break
        if accepted is None:
            raise TournamentError(f"cannot translate FSX move near {prefix!r}")
        width, candidate = accepted
        physical.extend(candidate)
        offset += width

    _cursor_from_physical(state, physical)
    return physical


def _cursor_from_physical(state: ArimaaState, steps: list[PhysicalStep]) -> TurnCursor:
    cursor = TurnCursor(state.board.copy())
    cursor.add_steps(steps)
    return cursor


class ProcessAdapter:
    def __init__(self, command: str, name: str, timeout: float = 30.0):
        expanded = os.path.expanduser(command)
        self.argv = shlex.split(expanded)
        if not self.argv:
            raise TournamentError(f"empty command for {name}")
        self.name = name
        self.timeout = timeout
        self.process: Optional[subprocess.Popen[str]] = None
        self.selector: Optional[selectors.BaseSelector] = None
        self._read_buffer = b""

    def start(self) -> None:
        self.process = subprocess.Popen(
            self.argv,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            bufsize=0,
        )
        assert self.process.stdout is not None
        self.selector = selectors.DefaultSelector()
        self.selector.register(self.process.stdout, selectors.EVENT_READ)

    def send(self, command: str) -> None:
        if self.process is None or self.process.stdin is None:
            raise TournamentError(f"{self.name} is not running")
        self.process.stdin.write((command + "\n").encode())
        self.process.stdin.flush()

    def read_line(self) -> str:
        if self.process is None or self.process.stdout is None or self.selector is None:
            raise TournamentError(f"{self.name} is not running")
        while b"\n" not in self._read_buffer:
            ready = self.selector.select(self.timeout)
            if not ready:
                raise TournamentError(f"timeout waiting for {self.name}")
            chunk = self.process.stdout.read(4096)
            if not chunk:
                code = self.process.poll()
                raise TournamentError(f"{self.name} exited unexpectedly ({code})")
            self._read_buffer += chunk

        line, self._read_buffer = self._read_buffer.split(b"\n", 1)
        return line.rstrip(b"\r").decode(errors="replace")

    @staticmethod
    def protocol_line(line: str) -> str:
        return line.lstrip("> ").strip()

    def wait_for(self, prefix: str) -> str:
        while True:
            line = self.protocol_line(self.read_line())
            if line.startswith(prefix):
                return line

    def close(self) -> None:
        if self.process is None:
            return
        try:
            self.send("quit")
            self.process.wait(timeout=2)
        except (OSError, subprocess.TimeoutExpired, TournamentError):
            self.process.kill()
        if self.selector is not None:
            self.selector.close()
        self.process = None
        self.selector = None
        self._read_buffer = b""


class FSXAdapter(ProcessAdapter):
    def __init__(self, command: str, variant_path: str, depth: int, timeout: float):
        super().__init__(command, "FSX", timeout)
        self.variant_path = variant_path
        self.depth = depth

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

    def bestmove(self, state: ArimaaState) -> str:
        self.send(fsx_position_command(state))
        self.send(f"go depth {self.depth}")
        line = self.wait_for("bestmove ")
        return line[len("bestmove "):].strip()


def fsx_position_command(state: ArimaaState) -> str:
    """Load a boundary position without discarding its repetition history."""

    if not state.history:
        return f"position fen {state.fen()}"
    return f"position fen {state.start_fen or STANDARD_SETUP_FEN} moves {' '.join(state.history)}"


class AEIAdapter(ProcessAdapter):
    def start(self) -> None:
        super().start()
        self.send("aei")
        self.wait_for("aeiok")
        self.send("isready")
        self.wait_for("readyok")
        self.send("newgame")

    def bestmove(self, state: ArimaaState) -> str:
        self.send(f"setposition {state.aei_position()}")
        self.send("go")
        line = self.wait_for("bestmove ")
        return line[len("bestmove "):].strip()


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fsx", required=True, help="FSX executable command")
    parser.add_argument("--akimot", required=True, help="Akimot AEI executable command")
    parser.add_argument("--variant-path", default="src/variants.ini")
    parser.add_argument("--depth", type=int, default=2, help="FSX fixed search depth")
    parser.add_argument("--games", type=int, default=2)
    parser.add_argument("--max-turns", type=int, default=500)
    parser.add_argument("--timeout", type=float, default=30.0)
    parser.add_argument("--log", type=Path, help="JSONL output file")
    return parser


def run_game(game_number: int, gold_engine: str, args: argparse.Namespace, log) -> dict:
    fsx = FSXAdapter(args.fsx, args.variant_path, args.depth, args.timeout)
    akimot = AEIAdapter(args.akimot, "Akimot", args.timeout)
    adapters = {"fsx": fsx, "akimot": akimot}
    state = ArimaaState.standard_setup()
    records: list[dict] = []
    try:
        fsx.start()
        akimot.start()
        assignments = {gold_engine: "g", "akimot" if gold_engine == "fsx" else "fsx": "s"}
        for turn_number in range(1, args.max_turns + 1):
            if state.winner is not None:
                break
            engine_name = next(name for name, side in assignments.items() if side == state.board.side)
            adapter = adapters[engine_name]
            raw = adapter.bestmove(state)
            if engine_name == "fsx":
                physical = parse_fsx_move(state, raw)
            else:
                physical = parse_aei_move(raw)
            result = state.apply_turn(physical)
            record = {
                "game": game_number,
                "turn": turn_number,
                "side": state.board.side,
                "engine": engine_name,
                "raw": raw,
                "aei": result.aei(),
                "fsx": result.fsx(),
                "fen": result.state.fen(),
                "position_key": result.state.board.key(),
                "winner": result.state.winner,
                "reason": result.state.reason,
            }
            records.append(record)
            if log is not None:
                log.write(json.dumps(record, sort_keys=True) + "\n")
                log.flush()
            state = result.state
        else:
            state.winner = None
            state.reason = "turn-limit"
    finally:
        fsx.close()
        akimot.close()

    summary = {
        "game": game_number,
        "gold": gold_engine,
        "silver": "akimot" if gold_engine == "fsx" else "fsx",
        "winner": state.winner,
        "reason": state.reason or "turn-limit",
        "turns": len(records),
    }
    if log is not None:
        log.write(json.dumps(summary, sort_keys=True) + "\n")
        log.flush()
    return summary


def main(argv: Optional[list[str]] = None) -> int:
    args = build_parser().parse_args(argv)
    if args.depth < 1 or args.games < 1 or args.max_turns < 1:
        raise SystemExit("depth, games, and max-turns must be positive")
    log = args.log.open("w", encoding="utf-8") if args.log else None
    try:
        summaries = []
        for game in range(1, args.games + 1):
            gold_engine = "fsx" if game % 2 else "akimot"
            summary = run_game(game, gold_engine, args, log)
            summaries.append(summary)
            print(json.dumps(summary, sort_keys=True))
        return 0
    finally:
        if log is not None:
            log.close()


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except TournamentError as error:
        print(f"tournament error: {error}", file=sys.stderr)
        raise SystemExit(2)
