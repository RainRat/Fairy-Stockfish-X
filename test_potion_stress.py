import subprocess

variants_append = """
[potion_overflow]
fairy = chess
potions = true
freezePotion = P
potionDropOnOccupied = true
# keeping maxRank/maxFile default 8x8
"""

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

# FEN: 8x8. We want to maximize moves on 8x8.
# Maybe empty board with one queen? 35 moves. 35 * 64 = 2240 moves.
# Still way below 65536.
# Actually, since it's an 8x8 board, MAX_MOVES is 16384 in ALLVARS.
# 16384 * 4 = 65536.
# If we have 32 queens, they block each other, reducing mobility.
# Wait! In ALLVARS, S=64. M <= 16384. M*S can exceed 65536 only if M > 1024.
# Is it possible to have >1024 moves on an 8x8 board in standard fairy chess?
# Let's consider a board where each square has a piece that can move to many squares, but wait, 8x8 = 64 squares.
# Maximum moves from ONE square is roughly 35 (Queen).
# 64 squares * 35 = 2240.
# Even if ALL 64 squares had a Queen that could magically not block each other, it's 2240 moves.
# 2240 * 64 (drop squares) = 143360.
# BUT pieces DO block each other.
# What about a piece that leaps anywhere? Amazon (Q+N) has 35+8=43 moves.
# What about pieces that can leap over others?
# What if we use a drop variant? `pieceDrops = true`.
# Hand size can be large. If we have 64 pieces in hand, we can drop them on 64 squares.
# 64 * 64 = 4096 drop moves.
# And then potion drop: 4096 * 64 = 262144 moves!
# Yes! `pieceDrops = true` allows a huge number of moves.
