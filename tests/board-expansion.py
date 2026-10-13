#!/usr/bin/env python3
"""512-bit checks; optional extra engine paths compare perft on overlapping boards."""
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
ENGINE = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ROOT / "src/stockfish-huge").resolve()
OUT = ROOT / ".local/build/board-expansion"
OUT.mkdir(parents=True, exist_ok=True)
INI = OUT / "variants.ini"
REFERENCES = []
for path in sys.argv[2:]:
    engine = pathlib.Path(path).resolve()
    info = subprocess.run([str(engine)], input="quit\n", text=True, capture_output=True,
                          check=True, timeout=60).stdout.splitlines()[0].split()
    limits = (22, 22) if "512" in info else (16, 16) if "VLB" in info else (12, 10) if "LB" in info else (8, 8)
    REFERENCES.append((engine, limits))


def fen(width, height, pieces, turn="w", pocket=""):
    rows = []
    for y in reversed(range(height)):
        row, empty = "", 0
        for x in range(width):
            pc = pieces.get((x, y))
            if pc:
                row += (str(empty) if empty else "") + pc
                empty = 0
            else:
                empty += 1
        rows.append(row + (str(empty) if empty else ""))
    return "/".join(rows) + pocket + f" {turn} - - 0 1"


def square(xy):
    x, y = xy
    return chr(97 + x) + str(y + 1)


def reference_moves(width, height, pieces, white):
    result = {}
    for (x, y), pc in pieces.items():
        if pc.isupper() != white:
            continue
        kind = pc.upper()
        if kind == "N":
            steps = [(a, b) for a, b in [(1, 2), (2, 1), (-1, 2), (-2, 1),
                                       (1, -2), (2, -1), (-1, -2), (-2, -1)]]
        else:
            steps = [(a, b) for a in (-1, 0, 1) for b in (-1, 0, 1) if a or b]
            if kind == "R":
                steps = [(a, b) for a, b in steps if not a or not b]
            elif kind == "B":
                steps = [(a, b) for a, b in steps if a and b]
        for dx, dy in steps:
            nx, ny = x + dx, y + dy
            while 0 <= nx < width and 0 <= ny < height:
                target = pieces.get((nx, ny))
                if target and target.isupper() == white:
                    break
                after = dict(pieces)
                del after[(x, y)]
                after[(nx, ny)] = pc
                result[square((x, y)) + square((nx, ny))] = after
                if target or kind in ("N", "K"):
                    break
                nx, ny = nx + dx, ny + dy
    return result


def perft(width, height, pieces, white, depth):
    if depth == 0:
        return 1
    return sum(perft(width, height, after, not white, depth - 1)
               for after in reference_moves(width, height, pieces, white).values())


def run(variant, commands, engine=ENGINE, protocol="uci"):
    text = (f"{protocol}\nsetoption name VariantPath value {INI}\n"
            f"setoption name UCI_Variant value {variant}\n"
            "setoption name Use NNUE value false\n" + commands + "\nquit\n")
    proc = subprocess.run([str(engine)], input=text, text=True, capture_output=True, timeout=60)
    assert proc.returncode == 0, proc.stderr + proc.stdout
    assert "runtime error:" not in proc.stderr, proc.stderr
    assert "No such variant" not in proc.stdout, proc.stdout
    return proc.stdout


def search(variant):
    proc = subprocess.Popen([str(ENGINE)], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True)
    try:
        proc.stdin.write(f"setoption name VariantPath value {INI}\n"
                         f"setoption name UCI_Variant value {variant}\n"
                         "setoption name Use NNUE value false\nposition startpos\ngo depth 2\n")
        proc.stdin.flush()
        # communicate after search completion: a reader thread allows a timeout.
        import concurrent.futures
        def read():
            lines = []
            for line in proc.stdout:
                lines.append(line)
                if line.startswith("bestmove "):
                    return "".join(lines)
            return "".join(lines)
        with concurrent.futures.ThreadPoolExecutor(1) as executor:
            future = executor.submit(read)
            try:
                result = future.result(timeout=60)
            except concurrent.futures.TimeoutError:
                proc.kill()
                raise
        assert re.search(r"bestmove [a-v]\d+[a-v]\d+", result), result
        assert "runtime error:" not in result, result
        assert "info depth 2 " in result, result
    finally:
        if proc.poll() is None:
            proc.stdin.write("quit\n")
            proc.stdin.flush()
        proc.wait(timeout=10)


dimensions = [(8, 8), (9, 9), (12, 10), (16, 16), (17, 17), (19, 19), (22, 22), (22, 17), (17, 22)]
configs = []
positions = {}
for width, height in dimensions:
    name = f"board-{width}x{height}"
    pieces = {(0, 0): "K", (1, 0): "R", (width - 1, height - 1): "k",
              (width - 2, height - 1): "r", (width // 2, height // 2): "N",
              (width // 2 - 1, height // 2 + 1): "b"}
    positions[name] = pieces
    configs.append(f"[{name}:chess]\nmaxFile = {chr(96 + width)}\nmaxRank = {height}\n"
                   f"castling = false\nallowChecks = true\nstartFen = {fen(width, height, pieces)}\n")
for name, extra in [("drops", "pieceDrops = true\ncapturesToHand = true\n"),
                    ("compound", "customPiece1 = a:K\ntwoStepMoves = a:*\n"),
                    ("hook", "customPiece1 = a:R\nhookMoves = a:R-R\n"),
                    ("wrap", "toroidal = true\n"),
                    ("royal", "allowChecks = false\n"),
                    ("capacity", "king = -\nstartFen = "
                     + "/".join(["N" * 22] * 22) + " w - - 0 1\n"),
                    ("promotion", "promotionRegionWhite = *22\npromotionRegionBlack = *1\n")]:
    configs.append(f"[{name}:board-22x22]\n{extra}")
INI.write_text("\n".join(configs))

for width, height in dimensions:
    name = f"board-{width}x{height}"
    start = fen(width, height, positions[name])
    output = run(name, "position startpos\nd\ngo perft 2\nd")
    assert output.count("Fen: " + start) == 2, output
    assert output.count("Sfen: " + start.split()[0] + " b - 1") == 2, output
    assert re.search(rf"^Nodes searched: {perft(width, height, positions[name], True, 2)}$", output, re.M), output
    divide = {m: int(n) for m, n in re.findall(r"^([a-v]\d+[a-v]\d+): (\d+)$", output, re.M)}
    expected = {move: len(reference_moves(width, height, after, False))
                for move, after in reference_moves(width, height, positions[name], True).items()}
    assert divide == expected, (name, divide, expected)
    for engine, (max_width, max_height) in REFERENCES:
        if width <= max_width and height <= max_height:
            baseline = run(name, "position startpos\nd\ngo perft 2\nd", engine=engine)
            baseline_divide = {m: int(n) for m, n in re.findall(r"^([a-v]\d+[a-v]\d+): (\d+)$", baseline, re.M)}
            assert baseline_divide == divide, (name, engine, baseline_divide, divide)
            assert baseline.count("Fen: " + start) == 2, (engine, baseline)
    keys = re.findall(r"^Key: (\w+)$", output, re.M)
    assert len(keys) == 2 and keys[0] == keys[1], output
    # Corner squares, including indices >= 256, survive move parsing and FEN round-trip.
    for coord in [(width - 1, height - 1), (0, height - 1), (width - 1, 0)]:
        pieces = {coord: "R", (1, 1): "K", (width - 2, height - 2): "k"}
        start = fen(width, height, pieces)
        output = run(name, f"position fen {start}\ngo perft 1")
        expected = reference_moves(width, height, pieces, True)
        actual = set(re.findall(r"^([a-v]\d+[a-v]\d+): 1$", output, re.M))
        assert actual == set(expected), (name, coord, actual, expected)
        move = next(m for m in expected if m.startswith(square(coord)))
        output = run(name, f"position fen {start} moves {move}\nd")
        assert "Fen: " + fen(width, height, expected[move], "b").split()[0] in output, output
    search(name)
    print(name, "reference perft, FEN, keys and search passed", flush=True)

if REFERENCES:
    print("Overlapping-board baseline perft and FEN comparisons passed", flush=True)

def check_special(variant, pieces, move, expected, pocket=""):
    start = fen(22, 22, pieces, pocket=pocket)
    output = run(variant, f"position fen {start}\nd\ngo perft 2\nd")
    assert re.search(r"^" + re.escape(move) + r": \d+$", output, re.M), output
    keys = re.findall(r"^Key: (\w+)$", output, re.M)
    assert keys[0] == keys[1] and output.count("Fen: " + start) == 2, output
    output = run(variant, f"position fen {start} moves {move}\nd")
    assert "Fen: " + fen(22, 22, expected, "b").split()[0] in output, output


kings = {(0, 0): "K", (21, 21): "k"}
check_special("promotion", {**kings, (20, 20): "P"}, "u21u22q", {**kings, (20, 21): "Q"})
check_special("drops", kings, "N@u22", {**kings, (20, 21): "N"}, "[N]")
check_special("compound", {**kings, (18, 15): "A", (19, 16): "p", (20, 17): "p"},
              "s16t17u18", {**kings, (20, 17): "A"})
check_special("hook", {**kings, (19, 18): "A"}, "t19t21u21", {**kings, (20, 20): "A"})
check_special("wrap", {**kings, (21, 20): "R"}, "v21a21", {**kings, (0, 20): "R"})
print("512-bit promotions, drops, compound moves, hooks, wrapping and undo passed")

pin = {(19, 19): "K", (19, 20): "R", (19, 21): "r", (0, 21): "k", (1, 0): "N"}
output = run("royal", f"position fen {fen(22, 22, pin)}\ngo perft 1")
assert "t21t22: 1" in output and "t21u21: 1" not in output, output
del pin[(19, 20)]
output = run("royal", f"position fen {fen(22, 22, pin)}\ngo perft 1")
assert "t20u20: 1" in output and "b1c3: 1" not in output, output
print("512-bit pins and check evasions above square 255 passed")

output = run("board-22x22", "position startpos\ngo perft 1", protocol="usi")
assert "21v21a: 1" in output, output
after = dict(positions["board-22x22"])
del after[(1, 0)]
after[(1, 21)] = "R"
output = run("board-22x22", "position startpos moves 21v21a\nd", protocol="usi")
assert "Fen: " + fen(22, 22, after, "b").split()[0] in output, output
print("512-bit USI coordinates passed")

dense = fen(22, 22, {(x, y): "N" for x in range(22) for y in range(22)})
output = run("capacity", f"position fen {dense}\nd\ngo perft 1\nd")
assert output.count("Fen: " + dense) == 2 and "Nodes searched: 0" in output, output
keys = re.findall(r"^Key: (\w+)$", output, re.M)
assert keys[0] == keys[1], output
print("512-bit 484-piece position capacity passed")

dense_pieces = {(x, y): "N" for x in range(22) for y in range(22)}
for x in range(18, 22):
    for y in range(18, 22):
        del dense_pieces[(x, y)]
dense_pieces[(21, 21)] = "n"
INI.write_text(INI.read_text() + "\n[dense-search:capacity]\nstartFen = "
               + fen(22, 22, dense_pieces) + "\n")
search("dense-search")
print("512-bit dense position search passed")
