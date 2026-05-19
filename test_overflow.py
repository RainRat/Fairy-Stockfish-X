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
"""
# 9 piece types in hand = 9 * 64 = 576 drop moves.
# Plus ~400 normal moves = 976 moves. 976 * 64 = 62464 moves. Still < 65536!
# We can use maxRank=10, maxFile=10, but LARGEBOARDS skips if we don't compile with LARGEBOARDS=yes.
# Is LARGEBOARDS=yes by default for python tests? Let's check test.py.
