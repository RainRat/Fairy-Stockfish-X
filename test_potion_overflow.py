import subprocess
import time

variants_append = """
[potion_overflow]
fairy = chess
potions = true
freezePotion = P
potionDropOnOccupied = true
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

# Empty board, with Potion (P) to drop. Let's see if we can trigger an overflow by having many pieces and we can drop a potion on each piece? Wait, freeze potion drop gates a normal move! So for EVERY valid normal move, we can do it on EVERY valid potion square.
# Number of normal moves: M
# Number of potion drop squares: S
# Total generated potion moves: M * S.
# S = 64
# M = 100
# 100 * 64 = 6400
# MOVEGEN_OVERFLOW_CAPACITY is MAX_MOVES * 4
# MAX_MOVES is 16384 (because ALLVARS is defined but not VERY_LARGE_BOARDS) or maybe 4096 (if ALLVARS is not defined)?
# Let's check MAX_MOVES in types.h: ALLVARS is defined by default? Yes in makefile ALLVARS is on for modern build?

# In ALLVARS, MAX_MOVES is 16384. 16384 * 4 = 65536.
# We need M * S > 65536.
# S <= 64. So M > 1024.
# Can we have a position with > 1024 normal moves?
# Yes, if we have lots of queens. Max moves in chess is ~218, but with many queens we can have more.
# 9 queens = ~240 moves. 14 queens = ~350 moves. 30 queens = ~800 moves.
# 60 queens on 8x8 is roughly: each queen has maybe 10-15 moves on average (due to being blocked by other queens, but if they can jump or capture...). Actually, if they are friendly pieces, they block each other, reducing mobility!
# To maximize moves, we need riders that can pass through pieces, or leapers.
# Or we can just use `allvars` build which means S <= 64. Wait! S is SQUARES on board!
# If we test on 12x10 board, S = 120. M can be larger.
# If we test on VERY_LARGE_BOARDS (16x16), S = 256.

engine.stdin.write("position fen 8/8/8/8/8/8/8/8 w - - 0 1\n") # will change this
