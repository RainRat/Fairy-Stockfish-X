import pyffish

variants_append = """
[potion_stress]
fairy = chess
potions = true
freezePotion = P
potionDropOnOccupied = true
maxRank = 10
maxFile = 12
customPiece1 = a:QN
customPiece2 = c:K
"""

with open("src/variants.ini", "a") as f:
    f.write(variants_append)

import subprocess
subprocess.run(["make", "build", "ARCH=x86-64-modern", "-j12", "-C", "src/"])

# build python extension
subprocess.run(["python3", "setup.py", "build_ext", "--inplace"])

import pyffish
pyffish.load_variant_config(variants_append)

variant = "potion_stress"

fen = "12/QQQQQQQQQQQQ/12/QQQQQQQQQQQQ/12/QQQQQQQQQQQQ/12/QQQQQQQQQQQQ/12/QQQQQQQQQQQQ w KQkq - 0 1"

try:
    print("Evaluating movegen...")
    moves = pyffish.legal_moves(variant, fen, [])
    print("Number of moves:", len(moves))
except Exception as e:
    print(f"Exception: {e}")

subprocess.run(["git", "checkout", "src/variants.ini"])
