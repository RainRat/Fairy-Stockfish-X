import pyffish
from pathlib import Path

# Need to manually get moves to see what is generated.
def repo_variants_ini():
    path = Path("src/variants.ini")
    return path if path.exists() else None

ini_text_expanded = """
[potion_overflow_test_expanded:chess]
potions = true
freezePotion = p
potionDropOnOccupied = true
pieceDrops = true
customPiece1 = a:QN
customPiece2 = c:K
customPiece3 = e:B
customPiece4 = h:R
customPiece5 = i:N
customPiece6 = m:Q
customPiece7 = o:R
customPiece8 = s:B
customPiece9 = t:N
customPiece10 = w:Q
customPiece11 = y:R
customPiece12 = z:B
customPiece13 = d:N
customPiece14 = g:Q
"""

path = repo_variants_ini()
pyffish.load_variant_config(path.read_text() + "\n" + ini_text_expanded)

fen = "K7/8/8/8/8/8/8/k7[PNBRQacehimoswtyzdg] w - - 0 1"
moves = pyffish.legal_moves("potion_overflow_test_expanded", fen, [])
print("Total moves:", len(moves))
# Check some moves
# Potion moves are drop moves on top of a base move.
# Like P@e4/e2e4
for i in range(10):
    print(pyffish.get_san("potion_overflow_test_expanded", fen, moves[i]))
