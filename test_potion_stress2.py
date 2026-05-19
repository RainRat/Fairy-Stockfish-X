import subprocess
import time

variants_append = """
[potion_overflow]
fairy = chess
potions = true
freezePotion = P
potionDropOnOccupied = true
pieceDrops = true
# 30 queens, each queen can move ~20 times => 600 moves.
# 600 moves * 64 drop squares = 38400. Not 65536 yet.
# What if we have drops in hand: 30 queens in hand.
# 30 queens in hand can be dropped on any of 64 squares? Wait, drop move is ONE move per square per piece TYPE.
# If you have 30 queens in hand, you still only generate ONE "drop Queen on X" move! So 64 moves.
# So having many of the same piece in hand doesn't multiply moves.
# What if we have many DIFFERENT pieces in hand? We have 5 piece types (P, N, B, R, Q).
# 5 * 64 = 320 moves. Still not enough.
# How to get > 1024 base moves?
# 64 squares. We need pieces that don't block each other.
# Knights. 32 knights on one color can move without blocking each other if the other color is empty, but they land on the OTHER color, which is empty, so they each have 8 moves!
# 32 knights * 8 moves = 256 moves.
# What about a piece that moves along lines but can jump over pieces?
# Custom piece: Amazon that jumps! No, standard fairy piece.
# Nightrider? A Nightrider can make multiple knight leaps in a line.
# But the board is only 8x8. Nightrider has max ~12 moves on an empty board.
# What if we use a HUGE board and LARGEBOARDS or VERY_LARGE_BOARDS?
# Is the claim that it can overflow on ALLVARS? The issue says "plausible, but proving it requires a stress setup with many potion-generated moves. ... build a stress variant that maximizes potion fanout on a large board or allvars build"
# Let's use maxRank=10, maxFile=12.
"""
