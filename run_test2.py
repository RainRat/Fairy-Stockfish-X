import pyffish
from pathlib import Path

# Wait, `potion_overflow_test_expanded` failed in my test because `sf.legal_moves` returned fewer than 60000.
# I got 311. Why 311?
# Let's generate a position that maximizes normal moves and see the count.
ini_text_expanded = """
[potion_overflow_test_expanded:chess]
potions = true
freezePotion = p
jumpPotion = n
potionDropOnOccupied = true
"""

path = Path("src/variants.ini")
pyffish.load_variant_config(path.read_text() + "\n" + ini_text_expanded)

# FEN that maximizes normal moves on 8x8.
# Maybe 8 queens on empty board?
# Q Q Q Q
#  Q Q Q Q
# Q Q Q Q
#  Q Q Q Q
fen = "Q1Q1Q1Q1/1Q1Q1Q1Q/Q1Q1Q1Q1/1Q1Q1Q1Q/4k3/8/8/K7 w - - 0 1"
moves = pyffish.legal_moves("potion_overflow_test_expanded", fen, [])
print("Total moves:", len(moves))
