#!/usr/bin/env python3
import sys
from pathlib import Path

# Add the helpers directory to sys.path to import Engine
helpers_dir = Path(__file__).resolve().parent.parent / ".local" / "scripts" / "helpers"
sys.path.append(str(helpers_dir))

from abmatch import Engine

def main():
    engine_path = "./src/stockfish"
    variant_path = "src/variants.ini"
    variant = "spell-chess"

    print("# Spell Chess Self-Play (Via AB Match Harness Engine)\n")
    print("This transcript is generated using the exact `Engine` wrapper and UCI communication loop")
    print("defined in the A/B match harness (`.local/scripts/helpers/abmatch.py`).\n")

    # Initialize two engine instances (White and Black) using the harness class
    print("Initializing engine instances...")
    white_engine = Engine(engine_path, variant_path, variant)
    black_engine = Engine(engine_path, variant_path, variant)
    print("Engines initialized successfully.\n")

    moves = []
    max_plies = 40

    for ply in range(max_plies):
        stm_white = (ply % 2 == 0)
        active_engine = white_engine if stm_white else black_engine
        
        # Get the best move using depth 6
        bm, blob = active_engine.go(moves, "go depth 6")
        
        # Get FEN by querying 'd'
        active_engine.cmd("position startpos" + ("" if not moves else " moves " + " ".join(moves)))
        active_engine.cmd("d")
        active_engine.e.expect(r"Fen:\s*(.*?)\r?\n")
        fen = active_engine.e.match.group(1).strip()
        
        # Expect rest of 'd' output up to "readyok" to clear the buffer
        active_engine.cmd("isready")
        active_engine.e.expect("readyok")

        player = "White" if stm_white else "Black"
        move_num = (ply // 2) + 1
        
        print(f"### Ply {ply + 1} (Move {move_num} - {player}): `{bm}`")
        print(f"**FEN:** `{fen}`\n")

        if bm in ("(none)", "none", "0000"):
            print("Game Over.")
            break
            
        moves.append(bm)

    print("## Final Move List")
    print(" ".join(moves))

    white_engine.close()
    black_engine.close()

if __name__ == "__main__":
    main()
