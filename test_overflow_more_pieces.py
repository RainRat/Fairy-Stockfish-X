import subprocess
import time

variants_append = """
[potion_overflow]
fairy = chess
potions = true
freezePotion = P
potionDropOnOccupied = true
pieceDrops = true
customPiece1 = a:QN
customPiece2 = c:K
customPiece3 = e:B
customPiece4 = h:R
customPiece5 = i:N
customPiece6 = m:Q
"""
# 11 piece types * 64 = 704 drops moves.
# 704 + 400 normal moves = 1104 moves.
# 1104 * 64 drop squares = 70656 > 65536! OVERFLOW!

with open("src/variants.ini", "a") as f:
    f.write(variants_append)

engine = subprocess.Popen(["src/stockfish"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
engine.stdin.write("setoption name VariantPath value src/variants.ini\n")
engine.stdin.write("isready\n")
engine.stdin.flush()

while True:
    line = engine.stdout.readline()
    if line.strip() == "readyok":
        break

engine.stdin.write("setoption name UCI_Variant value potion_overflow\n")
engine.stdin.write("isready\n")
engine.stdin.flush()

while True:
    line = engine.stdout.readline()
    if line.strip() == "readyok":
        break

# Empty board except 2 kings to be valid. All pieces in hand!
fen = "K7/8/8/8/8/8/8/k7[PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaacccccccccccccccccccccccccccccccceeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeehhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiimmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm] w - - 0 1"
# Wait, max hand size is MAX_DROP_PIECES which might be smaller.
# Let's just give 1 of each piece type in hand. 11 pieces in hand.
# 11 * 64 = 704 moves! YES! A single piece in hand allows drops on all 64 squares!
fen = "K7/8/8/8/8/8/8/k7[PNBRQacehim] w - - 0 1"

engine.stdin.write(f"position fen {fen}\n")
engine.stdin.write("go depth 1\n")
engine.stdin.flush()

time.sleep(2)
out, err = engine.communicate()
print("STDOUT:")
print(out)
print("STDERR:")
print(err)

subprocess.run(["git", "checkout", "src/variants.ini"])
