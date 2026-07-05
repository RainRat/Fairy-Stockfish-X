#!/usr/bin/env python3
"""Compare FSX laser-variant start moves with the pytactyx reference rules."""

import argparse
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def square(x, y, height, mirror=False):
    if mirror:
        x = 7 - x
    return f"{chr(ord('a') + x)}{height - y}"


def rotation_targets(piece):
    count = 2 if piece.kind == "SPLITTER" else 4
    current = piece.orient % count
    return [target for target in range(count) if target != current]


def simple_actions(rules, board, side, height, mirror=False, implicit_fire=False):
    result = {}
    for action in rules.generate_turns(board, side):
        rotation, move, fire = action
        if move:
            sx, sy, dx, dy, _promotion, move_kind = move
            text = square(sx, sy, height, mirror) + square(dx, dy, height, mirror)
            text += {"stack": "+", "unstack": "-", "swap": "s"}.get(move_kind, "")
        else:
            x, y, delta = rotation
            piece = board.occ[x][y]
            current = ((3 - piece.orient) if mirror else piece.orient) % 4
            target = ((3 - piece.orient - delta) if mirror else piece.orient + delta) % 4
            text = square(x, y, height, mirror) * 2
            if target != current:
                text += f":{target}"
            if fire and not implicit_fire:
                text += "f"
        result[text] = action
    return result


def load_fsx_board(board, fen, piece_kinds):
    """Load the board portion of an FSX laser FEN into a pytactyx Board."""
    from pytactyx.core.piece import Piece

    rows = fen.split()[0].split("/")
    for y, row in enumerate(rows):
        x = 0
        i = 0
        while i < len(row):
            if row[i].isdigit():
                j = i
                while j < len(row) and row[j].isdigit():
                    j += 1
                x += int(row[i:j])
                i = j
                continue
            symbol = row[i]
            i += 1
            orient = 0
            if i < len(row) and row[i] == ":":
                orient = int(row[i + 1])
                i += 2
            stacked = i < len(row) and row[i] == "+"
            if stacked:
                i += 1
            board.place(x, y, Piece(piece_kinds[symbol.lower()],
                                    1 if symbol.isupper() else 0,
                                    orient, 2 if stacked else 1))
            x += 1


def dos_actions(rules, board, side, variant_1994):
    result = {}
    moves = []
    rotations = []
    emitters = []
    for x in range(board.w):
        for y in range(board.h):
            piece = board.occ[x][y]
            if not piece or piece.side != side:
                continue
            if piece.kind in rules.ORIENTED:
                rotations.extend((x, y, target) for target in rotation_targets(piece))
            if piece.kind == "LASER":
                emitters.append((x, y))
            for dx, dy in rules.legal_moves(board, x, y, piece):
                moves.append((x, y, dx, dy, piece))

    for sx, sy, dx, dy, piece in moves:
        base = square(sx, sy, board.h) + square(dx, dy, board.h)
        result[base] = (None, (sx, sy, dx, dy, None, None), False)
        for rx, ry, target in rotations:
            gate_x, gate_y = (dx, dy) if (rx, ry) == (sx, sy) else (rx, ry)
            text = base + f":{target}" + square(gate_x, gate_y, board.h)
            result[text] = ((rx, ry, (target - board.occ[rx][ry].orient) % 4),
                            (sx, sy, dx, dy, None, None), False)

    if emitters:
        ex, ey = emitters[0]
        fire = square(ex, ey, board.h) * 2 + "f"
        result[fire] = (None, None, True)
        fire_rotations = rotations if variant_1994 else [r for r in rotations if r[:2] == (ex, ey)]
        for rx, ry, target in fire_rotations:
            text = square(rx, ry, board.h) * 2 + f":{target}f"
            result[text] = ((rx, ry, (target - board.occ[rx][ry].orient) % 4), None, True)

    if not variant_1994:
        for rx, ry, target in rotations:
            text = square(rx, ry, board.h) * 2 + f":{target}"
            result[text] = ((rx, ry, (target - board.occ[rx][ry].orient) % 4), None, False)
        result["0000"] = (None, None, False)
    return result


def compare(name, reference, allowed_extra=(), allowed_missing=()):
    import pyffish

    fsx = set(pyffish.legal_moves(name, pyffish.start_fen(name), []))
    ref = set(reference)
    extra = fsx - ref - set(allowed_extra)
    missing = ref - fsx - set(allowed_missing)
    if extra or missing:
        print(f"{name}: mismatch ({len(fsx)} FSX, {len(ref)} pytactyx)")
        if extra:
            print("  FSX only:", " ".join(sorted(extra)))
        if missing:
            print("  pytactyx only:", " ".join(sorted(missing)))
        return False
    exceptions = (fsx - ref) | (ref - fsx)
    note = f", {len(exceptions)} documented exception" if exceptions else ""
    print(f"{name}: {len(fsx)} moves matched{note}")
    return True


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--pytactyx", type=Path,
                        default=Path.home() / "gem_workspace" / "pytactyx")
    args = parser.parse_args()
    sys.path.insert(0, str(args.pytactyx))
    sys.path.insert(0, str(ROOT))

    import pyffish
    from pytactyx.core.board import Board
    from pytactyx.games.chess_variants.dos_chess_rules import DosChessRuleset
    from pytactyx.games.laser_games.khet1_rules import Khet1Ruleset
    from pytactyx.games.laser_games.khet2_rules import Khet2Ruleset
    from pytactyx.games.laser_games.laser_chess_94_rules import LaserChess94Ruleset
    from pytactyx.games.laser_games.playlaser_rules import PlaylaserRuleset

    pyffish.load_variant_config((ROOT / "src" / "variants.ini").read_text())
    cases = []
    for name, cls, mirror in (
        ("khet1", Khet1Ruleset, False),
        ("khet2", Khet2Ruleset, False),
        ("playlaser", PlaylaserRuleset, True),
    ):
        rules = cls()
        board = Board(rules)
        rules.setup_initial(board)
        cases.append((name, simple_actions(rules, board, 1, board.h, mirror,
                                           implicit_fire=name.startswith("khet"))))

    for name, cls, is_1994 in (
        ("dos-laser-chess", DosChessRuleset, False),
        ("dos-laser-chess-1994", LaserChess94Ruleset, True),
    ):
        rules = cls()
        board = Board(rules)
        load_fsx_board(board, pyffish.start_fen(name), {
            "r": "ROOK", "s": "SPLITTER", "b": "BISHOP", "q": "QUEEN",
            "k": "KING", "l": "LASER", "m": "MIRROR", "d": "SHIELD",
            "p": "SUPER",
        })
        cases.append((name, dos_actions(rules, board, 1, is_1994)))

    ok = True
    for name, actions in cases:
        # Pytactyx currently permits the white Khet 2 Sphinx to rotate toward
        # the board. The physical corner restriction allows only orientations
        # 0 and 1, so FSX intentionally omits j1j1:3.
        missing = {"j1j1:3"} if name == "khet2" else set()
        ok &= compare(name, actions, allowed_missing=missing)
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
