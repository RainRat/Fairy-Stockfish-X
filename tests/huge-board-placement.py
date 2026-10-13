#!/usr/bin/env python3
"""Shipped huge-board placement rules, boundary moves, undo, and search."""
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
ENGINE = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ROOT / "src/stockfish-huge").resolve()
INI = pathlib.Path(sys.argv[2] if len(sys.argv) > 2 else ROOT / "src/variants.ini").resolve()


def run(variant, commands):
    script = (f"uci\nsetoption name VariantPath value {INI}\n"
              f"setoption name UCI_Variant value {variant}\n"
              "setoption name Use NNUE value false\n" + commands + "\nquit\n")
    proc = subprocess.run([str(ENGINE)], input=script, text=True, capture_output=True, timeout=60)
    assert proc.returncode == 0, proc.stdout + proc.stderr
    assert "Invalid" not in proc.stdout and "runtime error:" not in proc.stderr, proc.stdout + proc.stderr
    assert "No such variant" not in proc.stdout, proc.stdout
    return proc.stdout


def nodes(output):
    return [int(n) for n in re.findall(r"^Nodes searched: (\d+)$", output, re.M)]


def fen(size, pieces, turn="w", points=""):
    rows = []
    for y in reversed(range(size)):
        row, empty = "", 0
        for x in range(size):
            piece = pieces.get((x, y))
            if piece:
                row += (str(empty) if empty else "") + piece
                empty = 0
            else:
                empty += 1
        rows.append(row + (str(empty) if empty else ""))
    return "/".join(rows) + f" {turn} - - 0 1" + points


catalog = subprocess.run([str(ENGINE)], input=f"uci\nsetoption name VariantPath value {INI}\nuci\nquit\n",
                         text=True, capture_output=True, check=True, timeout=60).stdout
huge = " HB 512 " in catalog
available = set(re.findall(r"\bvar ([^\s]+)", catalog))
cases = [("gomoku-19x19", 19, False), ("hex-19x19", 19, False), ("hex-22x22", 22, False),
         ("go19", 19, True), ("capture-go19", 19, False), ("nogo19", 19, False),
         ("suicide-go19", 19, True)]
for name, size, passing in cases:
    if name not in available:
        assert not huge, f"Huge boards engine is missing {name}"
        print(f"FSX_TEST_PARTIAL_SKIP: {name} requires hugeboards=yes", flush=True)
        continue
    output = run(name, "position startpos\nd\ngo perft 2\nd")
    root_moves = re.findall(r"^(P@[a-v]\d+|0000): (\d+)$", output, re.M)
    assert len(root_moves) == size * size + passing, (name, output)
    initial = re.findall(r"^Fen: (.+)$", output, re.M)
    keys = re.findall(r"^Key: (\w+)$", output, re.M)
    assert len(initial) == 2 and initial[0] == initial[1], output
    assert len(keys) == 2 and keys[0] == keys[1], output
    corner = f"P@{chr(96 + size)}{size}"
    assert corner in dict(root_moves), output
    moved = run(name, f"position startpos moves {corner}\nd\nbench 16 1 1 current depth")
    assert re.search(r"^bestmove P@", moved, re.M), moved
    placement = re.search(r"^Fen: (.+)$", moved, re.M)[1].split()[0]
    assert placement.startswith(str(size - 1) + ("P" if name.startswith("gomoku") else "p")), moved
    print(name, "initial perft, boundary drop, FEN/key undo and search passed", flush=True)

if not huge:
    sys.exit(0)


def check_drop(name, pieces, move, legal=True, terminal=False, points="", size=19, turn="w"):
    start = fen(size, pieces, turn=turn, points=points)
    output = run(name, f"position fen {start}\ngo perft 1")
    assert bool(re.search(r"^" + re.escape(move) + r": 1$", output, re.M)) == legal, output
    if legal:
        output = run(name, f"position fen {start}\nd\ngo perft 2\nd\nposition fen {start} moves {move}\nd\ngo perft 1")
        keys = re.findall(r"^Key: (\w+)$", output, re.M)
        assert keys[0] == keys[1], output
        if terminal:
            assert nodes(output)[-1] == 0, output
        return re.findall(r"^Fen: (.+)$", output, re.M)[-1]


check_drop("gomoku-19x19", {(x, 18): "P" for x in range(14, 18)}, "P@s19", terminal=True)
check_drop("hex-19x19", {(x, 18): "P" for x in range(18)}, "P@s19", terminal=True)
check_drop("hex-19x19", {(18, y): "p" for y in range(18)}, "P@s19", terminal=True, turn="b")
check_drop("hex-22x22", {(x, 21): "P" for x in range(21)}, "P@v22", terminal=True, size=22)
check_drop("hex-22x22", {(21, y): "p" for y in range(21)}, "P@v22", terminal=True, size=22, turn="b")
corner = {(18, 18): "p", (17, 18): "P"}
check_drop("capture-go19", corner, "P@s18", terminal=True, points=" {0 0}")
check_drop("nogo19", corner, "P@s18", legal=False)
almost_full = {(x, y): "P" for x in range(19) for y in range(19) if (x, y) != (18, 18)}
assert nodes(run("nogo19", f"position fen {fen(19, almost_full, turn='b')}\ngo perft 1")) == [0]
surround = {(17, 18): "p", (18, 17): "p"}
check_drop("go19", surround, "P@s19", legal=False)
removed = check_drop("suicide-go19", surround, "P@s19")
assert removed.split()[0] == fen(19, surround).split()[0], removed
empty = fen(19, {})
assert nodes(run("go19", f"position fen {empty} moves 0000 0000\ngo perft 1")) == [0]
print("Huge-board connection wins, liberty rules and two-pass ending passed")
