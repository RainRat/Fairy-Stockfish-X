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


def fsx_fen(board, piece_chars, side=1):
    rows = []
    for y in range(board.h):
        row = []
        empty = 0
        for x in range(board.w):
            piece = board.occ[x][y]
            if not piece:
                empty += 1
                continue
            if empty:
                row.append(str(empty))
                empty = 0
            symbol = piece_chars[piece.kind]
            symbol = symbol.upper() if piece.side == 1 else symbol.lower()
            row.append(symbol)
            if piece.kind in board.ruleset.ORIENTED:
                row.append(f":{piece.orient % 4}")
            if piece.stack_height > 1:
                row.append("+")
        if empty:
            row.append(str(empty))
        rows.append("".join(row))
    return "/".join(rows) + (" w - - 0 1" if side == 1 else " b - - 0 1")


def normalized_board(board, mirror=False, compare_orient=True):
    pieces = []
    for x in range(board.w):
        for y in range(board.h):
            piece = board.occ[x][y]
            if not piece:
                continue
            nx = board.w - 1 - x if mirror else x
            orient = 0
            if piece.kind in board.ruleset.ORIENTED and compare_orient:
                orient = ((3 - piece.orient) if mirror else piece.orient) % 4
            pieces.append((nx, y, piece.kind, piece.side, orient, piece.stack_height))
    return tuple(sorted(pieces))


def compare_outcomes(name, rules, board, actions, piece_kinds, mirror=False,
                     ignored=()):
    import pyffish
    from pytactyx.core.board import Board

    start = pyffish.start_fen(name)
    failures = []
    checked = 0
    for move, action in sorted(actions.items()):
        if move in ignored:
            continue
        reference = board.clone()
        try:
            rules.apply_turn(reference, action, 1)
            actual_fen = pyffish.get_fen(name, start, [move])
            actual = Board(rules)
            load_fsx_board(actual, actual_fen, piece_kinds)
        except Exception as exc:  # Keep a complete, actionable mismatch report.
            failures.append((move, f"execution error: {exc}"))
            continue
        checked += 1
        expected_state = normalized_board(reference, mirror, not mirror)
        actual_state = normalized_board(actual, compare_orient=not mirror)
        if expected_state != actual_state:
            expected_set = set(expected_state)
            actual_set = set(actual_state)
            failures.append((move,
                f"FSX-only={sorted(actual_set - expected_set)} "
                f"pytactyx-only={sorted(expected_set - actual_set)}"))
    if failures:
        print(f"{name}: {len(failures)} outcome mismatches after {checked} actions")
        for move, detail in failures[:20]:
            print(f"  {move}: {detail}")
        if len(failures) > 20:
            print(f"  ... {len(failures) - 20} more")
        return False
    print(f"{name}: {checked} one-ply outcomes matched")
    return True


def compare_interaction(name, rules, board, action, move, piece_kinds, piece_chars):
    import pyffish
    from pytactyx.core.board import Board

    before = fsx_fen(board, piece_chars)
    reference = board.clone()
    rules.apply_turn(reference, action, 1)
    try:
        after = pyffish.get_fen(name, before, [move])
    except Exception as exc:
        return f"FSX rejected {move}: {exc}"
    actual = Board(rules)
    load_fsx_board(actual, after, piece_kinds)
    expected_state = normalized_board(reference)
    actual_state = normalized_board(actual)
    if expected_state == actual_state:
        return None
    expected_set = set(expected_state)
    actual_set = set(actual_state)
    return (f"FSX-only={sorted(actual_set - expected_set)} "
            f"pytactyx-only={sorted(expected_set - actual_set)}")


def interaction_matrix(pyffish, Board, Piece, piece_maps):
    from pytactyx.games.chess_variants.dos_chess_rules import DosChessRuleset
    from pytactyx.games.laser_games.khet1_rules import Khet1Ruleset
    from pytactyx.games.laser_games.khet2_rules import Khet2Ruleset

    failures = []
    checked = 0
    dos_chars = {"ROOK": "r", "SPLITTER": "s", "BISHOP": "b",
                 "QUEEN": "q", "KING": "k", "LASER": "l",
                 "MIRROR": "m", "SHIELD": "d", "SUPER": "p"}
    for name, rules in (("dos-laser-chess", DosChessRuleset()),):
        for kind in rules.KINDS:
            if kind == "LASER":
                continue
            orientations = (range(2) if kind == "SPLITTER" else
                            range(4) if kind in rules.ORIENTED else (0,))
            for orient in orientations:
                board = Board(rules)
                board.place(0, 5, Piece("LASER", 1, 1))       # a4, firing east
                board.place(4, 5, Piece(kind, 0, orient))       # e4, test target
                board.place(4, 2, Piece("SHIELD", 0, 0))       # e7
                board.place(4, 8, Piece("SHIELD", 0, 2))       # e1
                board.place(7, 5, Piece("SHIELD", 0, 3))       # h4
                if kind != "KING":
                    board.place(8, 0, Piece("KING", 0, 0))     # i9
                board.place(8, 8, Piece("KING", 1, 0))         # i1
                error = compare_interaction(name, rules, board, (None, None, True),
                                            "a4a4f", piece_maps["dos"], dos_chars)
                checked += 1
                if error:
                    failures.append((f"{name}:{kind}:{orient}", error))

    khet_chars = {"PYRAMID": "p", "DJED": "s", "OBELISK": "o",
                  "PHARAOH": "k", "EYE_OF_HORUS": "e"}
    for name, rules in (("khet1", Khet1Ruleset()), ("khet2", Khet2Ruleset())):
        kinds = ["PYRAMID", "PHARAOH", "EYE_OF_HORUS"]
        kinds += ["DJED", "OBELISK"] if name == "khet1" else ["SCARAB", "ANUBIS", "SPHINX"]
        chars = dict(khet_chars)
        if name == "khet2":
            chars.update({"SCARAB": "s", "ANUBIS": "a", "SPHINX": "x"})
        targets = [(kind, 1) for kind in kinds]
        if name == "khet1":
            targets.append(("OBELISK", 2))
        for kind, stack_height in targets:
            orientations = range(4) if kind in rules.ORIENTED else (0,)
            for orient in orientations:
                board = Board(rules)
                board.place(1, 6, Piece("PYRAMID", 1, 0))      # b2 action dummy
                board.place(2, 7, Piece("PHARAOH", 1, 0))      # c1
                if name == "khet2":
                    board.place(9, 7, Piece("SPHINX", 1, 0))   # j1 emitter
                if kind == "PHARAOH":
                    board.place(9, 4, Piece(kind, 0, orient, stack_height))
                else:
                    board.place(2, 0, Piece("PHARAOH", 0, 0))  # c8
                    board.place(9, 4, Piece(kind, 0, orient, stack_height))
                board.place(6, 4, Piece("OBELISK" if name == "khet1" else "ANUBIS", 0, 0))
                board.place(9, 1, Piece("OBELISK" if name == "khet1" else "ANUBIS", 0, 0))
                action = ((1, 6, 1), None, True)
                error = compare_interaction(name, rules, board, action, "b2b2:1",
                                            piece_maps[name], chars)
                checked += 1
                if error:
                    failures.append((f"{name}:{kind}:{orient}:stack{stack_height}", error))
    if failures:
        print(f"interaction matrix: {len(failures)} mismatches after {checked} cases")
        for case, error in failures:
            print(f"  {case}: {error}")
        return False
    print(f"interaction matrix: {checked} piece/face cases matched")
    return True


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
            if variant_1994:
                text = square(ex, ey, board.h) * 2 + f":{target}"
                if (rx, ry) != (ex, ey):
                    text += square(rx, ry, board.h)
                text += "f"
            else:
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
    parser.add_argument("--moves-only", action="store_true",
                        help="skip applying and comparing every one-ply outcome")
    args = parser.parse_args()
    sys.path.insert(0, str(args.pytactyx))
    sys.path.insert(0, str(ROOT))

    import pyffish
    from pytactyx.core.board import Board
    from pytactyx.core.piece import Piece
    from pytactyx.games.chess_variants.dos_chess_rules import DosChessRuleset
    from pytactyx.games.laser_games.khet1_rules import Khet1Ruleset
    from pytactyx.games.laser_games.khet2_rules import Khet2Ruleset
    from pytactyx.games.laser_games.laser_chess_94_rules import LaserChess94Ruleset
    from pytactyx.games.laser_games.playlaser_rules import PlaylaserRuleset

    pyffish.load_variant_config((ROOT / "src" / "variants.ini").read_text())
    cases = []
    piece_maps = {
        "khet1": {"p": "PYRAMID", "s": "DJED", "o": "OBELISK",
                   "d": "OBELISK", "k": "PHARAOH", "e": "EYE_OF_HORUS"},
        "khet2": {"p": "PYRAMID", "s": "SCARAB", "a": "ANUBIS",
                   "x": "SPHINX", "k": "PHARAOH", "e": "EYE_OF_HORUS"},
        "playlaser": {"l": "LASER", "w": "WALL", "n": "KNIGHT",
                      "p": "PAWN", "k": "KING"},
        "dos": {"r": "ROOK", "s": "SPLITTER", "b": "BISHOP",
                "q": "QUEEN", "k": "KING", "l": "LASER",
                "m": "MIRROR", "d": "SHIELD", "p": "SUPER"},
    }
    for name, cls, mirror in (
        ("khet1", Khet1Ruleset, False),
        ("khet2", Khet2Ruleset, False),
        ("playlaser", PlaylaserRuleset, True),
    ):
        rules = cls()
        board = Board(rules)
        if name.startswith("khet"):
            load_fsx_board(board, pyffish.start_fen(name), piece_maps[name])
        else:
            rules.setup_initial(board)
        cases.append((name, rules, board,
                      simple_actions(rules, board, 1, board.h, mirror,
                                     implicit_fire=name.startswith("khet")),
                      piece_maps[name], mirror))

    for name, cls, is_1994 in (
        ("dos-laser-chess", DosChessRuleset, False),
        ("dos-laser-chess-1994", LaserChess94Ruleset, True),
    ):
        rules = cls()
        board = Board(rules)
        load_fsx_board(board, pyffish.start_fen(name), piece_maps["dos"])
        cases.append((name, rules, board, dos_actions(rules, board, 1, is_1994),
                      piece_maps["dos"], False))

    ok = True
    for name, rules, board, actions, piece_kinds, mirror in cases:
        # Pytactyx labels both Sphinx rotation inputs even when its corner
        # constraint advances them to the same inward-facing orientation.
        missing = {"j1j1:1"} if name == "khet2" else set()
        ok &= compare(name, actions, allowed_missing=missing)
        if not args.moves_only:
            ok &= compare_outcomes(name, rules, board, actions, piece_kinds, mirror,
                                   ignored=missing)
    if not args.moves_only:
        ok &= interaction_matrix(pyffish, Board, Piece, piece_maps)
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
