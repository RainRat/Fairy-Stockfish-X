import subprocess
import time

variants_append = """
[potion_overflow]
fairy = chess
potions = true
freezePotion = P
potionDropOnOccupied = true
maxRank = 10
maxFile = 10
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

# 10x10 board.
# Maximize moves by placing Queens.
# 10 Queens on ranks 1,3,5,7,9.
# 10/QQQQQQQQQQ/10/QQQQQQQQQQ/10/QQQQQQQQQQ/10/QQQQQQQQQQ/10/QQQQQQQQQQ
fen = "10/QQQQQQQQQQ/10/QQQQQQQQQQ/10/QQQQQQQQQQ/10/QQQQQQQQQQ/10/QQQQQQQQQQ w - - 0 1"

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
