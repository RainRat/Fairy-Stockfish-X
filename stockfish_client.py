# -*- coding: utf-8 -*-

import subprocess
import sys
import threading
import queue
import time
import os
from pathlib import Path

# Try importing pyffish, but don't fail immediately so we can print a nice error
try:
    import pyffish as sf
except ImportError:
    sf = None


class StockfishEngine:
    """
    Manages the lifecycle of the Stockfish engine subprocess and handles
    asynchronous communication over stdin/stdout/stderr.
    """
    def __init__(self, engine_path="./src/stockfish", variants_path="src/variants.ini"):
        self.engine_path = engine_path
        self.variants_path = variants_path

        if not os.path.exists(engine_path):
            # Try repository root fallback
            if os.path.exists("./stockfish"):
                self.engine_path = "./stockfish"
            else:
                raise FileNotFoundError(
                    f"Stockfish engine not found at '{engine_path}' or './stockfish'. "
                    "Please compile it using 'make build' inside the src directory first."
                )

        self.proc = subprocess.Popen(
            [self.engine_path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1
        )

        self.out_queue = queue.Queue()
        self.err_queue = queue.Queue()

        self.stdout_thread = threading.Thread(
            target=self._read_stream, args=(self.proc.stdout, self.out_queue), daemon=True
        )
        self.stderr_thread = threading.Thread(
            target=self._read_stream, args=(self.proc.stderr, self.err_queue), daemon=True
        )
        self.stdout_thread.start()
        self.stderr_thread.start()

        # Initialize UCI
        self.send("uci")
        self.read_until("uciok")

        # Load variants.ini if it exists
        if os.path.exists(self.variants_path):
            self.send(f"setoption name VariantPath value {self.variants_path}")
            self.send("isready")
            self.read_until("readyok")

    def _read_stream(self, stream, q):
        try:
            for line in iter(stream.readline, ''):
                q.put(line.strip())
        except Exception:
            pass

    def send(self, cmd):
        """Sends a command to the stockfish engine stdin."""
        self.proc.stdin.write(cmd + "\n")
        self.proc.stdin.flush()

    def read_line(self, timeout=0.1):
        """Reads a line from stdout queue with timeout."""
        try:
            return self.out_queue.get(timeout=timeout)
        except queue.Empty:
            return None

    def read_err_line(self, timeout=0.1):
        """Reads a line from stderr queue with timeout."""
        try:
            return self.err_queue.get(timeout=timeout)
        except queue.Empty:
            return None

    def read_until(self, end_str, timeout=5.0):
        """Reads lines from stdout until a line starts with or contains end_str."""
        lines = []
        start_time = time.time()
        while time.time() - start_time < timeout:
            line = self.read_line(timeout=0.05)
            if line is not None:
                lines.append(line)
                if line.startswith(end_str) or end_str in line:
                    break
        return lines

    def flush(self):
        """Discards all pending lines in stdout and stderr queues."""
        while not self.out_queue.empty():
            try:
                self.out_queue.get_nowait()
            except queue.Empty:
                break
        while not self.err_queue.empty():
            try:
                self.err_queue.get_nowait()
            except queue.Empty:
                break

    def set_variant(self, variant_name):
        """Configures the engine to use the specified variant."""
        self.flush()
        self.send(f"setoption name UCI_Variant value {variant_name}")
        self.send("isready")
        return self.read_until("readyok")

    def get_best_move(self, fen, movelist=None, movetime_ms=1000):
        """
        Sets position in the engine and searches for the best move.
        Returns the bestmove string.
        """
        self.flush()
        if movelist:
            moves_str = " ".join(movelist)
            self.send(f"position fen {fen} moves {moves_str}")
        else:
            self.send(f"position fen {fen}")

        self.send(f"go movetime {movetime_ms}")
        lines = self.read_until("bestmove", timeout=(movetime_ms / 1000.0) + 3.0)
        for line in lines:
            if line.startswith("bestmove"):
                parts = line.split()
                if len(parts) >= 2:
                    return parts[1]
        return None

    def quit(self):
        """Quits the stockfish engine subprocess."""
        try:
            self.send("quit")
            self.proc.terminate()
        except Exception:
            pass


def print_banner():
    print("=" * 65)
    print("      FAIRY-STOCKFISH-X PYTHON TERMINAL CLIENT")
    print("=" * 65)
    if sf is None:
        print(" [WARNING] pyffish library is not loaded!")
        print(" Please run: python3 setup.py build_ext --inplace")
        print("=" * 65)


def clear_screen():
    os.system('cls' if os.name == 'nt' else 'clear')


def get_board_rendering(engine, fen, movelist):
    """
    Sends the position and 'd' command to the engine, parses the output,
    and returns a formatted board visualization string.
    """
    engine.flush()
    if movelist:
        engine.send(f"position fen {fen} moves {' '.join(movelist)}")
    else:
        engine.send(f"position fen {fen}")
    engine.send("d")

    # Read the engine's response until we get the Fen/Sfen details
    lines = engine.read_until("Fen: ", timeout=2.0)

    board_lines = []
    capture = False
    for line in lines:
        # The board starts with a line containing +---+ and ends around the file letters (e.g. a b c d)
        # and has rank indicators like | or numbers
        stripped = line.strip()
        if stripped.startswith("+---") or stripped.startswith("+-"):
            capture = True
        if capture:
            board_lines.append(line)
        if line.startswith("Fen:") or line.startswith("Sfen:") or "Key:" in line:
            # We hit the end of the board description
            break

    # Clean up and join
    return "\n".join(board_lines[:-1]) if board_lines else "No board visualization available from engine."


def select_variant_flow():
    if sf is None:
        print("Error: pyffish must be compiled to use this client.")
        sys.exit(1)

    all_variants = sorted(list(sf.variants()))
    popular_variants = [v for v in ["chess", "crazyhouse", "antichess", "atomic", "capablanca", "shogi", "xiangqi", "racingkings"] if v in all_variants]

    while True:
        print("\n--- SELECT CHESS VARIANT ---")
        print("Popular Variants:")
        for idx, var in enumerate(popular_variants, 1):
            print(f"  {idx}. {var}")
        print("  9. See complete list of all variants")
        print("  0. Quit")

        choice = input("\nEnter choice or type variant name: ").strip()
        if choice == "0":
            sys.exit(0)
        elif choice == "9":
            print("\nAll Available Variants:")
            # Print in columns
            cols = 4
            for i in range(0, len(all_variants), cols):
                chunk = all_variants[i:i+cols]
                print(" | ".join(f"{v:<15}" for v in chunk))
            print()
            continue

        if choice.isdigit():
            val = int(choice)
            if 1 <= val <= len(popular_variants):
                return popular_variants[val - 1]
            else:
                print("Invalid popular variant index.")
        elif choice in all_variants:
            return choice
        else:
            # Fuzzy match
            matches = [v for v in all_variants if choice.lower() in v.lower()]
            if len(matches) == 1:
                confirm = input(f"Did you mean '{matches[0]}'? (y/n): ").strip().lower()
                if confirm == 'y':
                    return matches[0]
            elif len(matches) > 1:
                print("Multiple matches found:")
                for m in matches:
                    print(f"  - {m}")
            else:
                print(f"Unknown variant '{choice}'.")


def run_direct_uci_mode(engine):
    print("\n" + "=" * 50)
    print(" DIRECT UCI CONSOLE MODE")
    print(" Type raw UCI commands directly to stockfish.")
    print(" Type 'exit' to return to the main game menu.")
    print("=" * 50)

    # Enable non-blocking reading in background
    while True:
        cmd = input("UCI> ").strip()
        if not cmd:
            continue
        if cmd.lower() in ["exit", "quit", "q"]:
            print("Exiting UCI console mode.")
            break

        engine.send(cmd)
        # Sleep slightly to let the engine process and dump output
        time.sleep(0.1)
        # Flush stdout queue
        while True:
            line = engine.read_line(timeout=0.05)
            if line is None:
                break
            print(line)
        # Flush stderr queue
        while True:
            err_line = engine.read_err_line(timeout=0.05)
            if err_line is None:
                break
            print(f"[Engine Error] {err_line}", file=sys.stderr)


def get_side_to_move(variant, fen, movelist):
    """Determines which color's turn it is: 'white' or 'black'."""
    current_fen = sf.get_fen(variant, fen, movelist)
    # FEN format usually has side to move as second field: 'w' or 'b'
    parts = current_fen.split()
    if len(parts) >= 2:
        return 'white' if parts[1] == 'w' else 'black'
    return 'white'


def export_game(variant, start_fen, movelist, moves_san, result_str):
    """Exports the game details to a file."""
    filename = f"game_{variant}_{int(time.time())}.txt"
    with open(filename, "w", encoding="utf-8") as f:
        f.write(f"Variant: {variant}\n")
        f.write(f"StartFEN: {start_fen}\n")
        f.write(f"Result: {result_str}\n\n")
        f.write("Moves (LAN / Coordinate Notation):\n")
        f.write(" ".join(movelist) + "\n\n")
        f.write("Moves (SAN / Standard Algebraic Notation):\n")

        # Format SAN nicely with move numbers
        formatted_san = []
        is_white_turn = start_fen.split()[1] == 'w' if len(start_fen.split()) >= 2 else True
        move_num = int(start_fen.split()[5]) if len(start_fen.split()) >= 6 else 1

        for i, san in enumerate(moves_san):
            if is_white_turn:
                formatted_san.append(f"{move_num}. {san}")
                is_white_turn = False
            else:
                formatted_san.append(san)
                move_num += 1
                is_white_turn = True

        f.write(" ".join(formatted_san) + "\n")
    print(f"\n[INFO] Game exported successfully to {filename}")


def play_game(engine, variant):
    print(f"\nInitializing game for variant: '{variant}'...")
    engine.set_variant(variant)

    start_fen = sf.start_fen(variant)
    movelist = []
    moves_san = []

    print("\nChoose Game Mode:")
    print("  1. Play as White (Computer plays Black)")
    print("  2. Play as Black (Computer plays White)")
    print("  3. Player vs Player (Manual control for both)")
    print("  4. Selfplay (Computer plays both / watch only)")
    print("  5. Direct UCI command entry mode")
    print("  0. Go back to variant selection")

    mode_choice = input("\nEnter choice: ").strip()
    if mode_choice == "0":
        return
    elif mode_choice == "5":
        run_direct_uci_mode(engine)
        return

    # Map roles
    white_is_comp = False
    black_is_comp = False
    self_play = False

    if mode_choice == "1":
        black_is_comp = True
    elif mode_choice == "2":
        white_is_comp = True
    elif mode_choice == "3":
        pass
    elif mode_choice == "4":
        self_play = True
    else:
        print("Invalid choice, defaulting to Player vs Player.")

    # Game Loop
    game_over = False
    result_str = "*"

    # Initial Draw
    print("\n" + "=" * 50)
    print(f" PLAYING {variant.upper()}")
    print(" Commands during play:")
    print("   Type move (e.g. 'e4', 'Nf3', 'e2e4') to make a move.")
    print("   Type '/draw' or '/d' to redraw the board.")
    print("   Type '/hint' to get the best move from Stockfish.")
    print("   Type '/uci' to drop into direct UCI Console.")
    print("   Type '/export' to export current game.")
    print("   Type '/exit' to abort the current game.")
    print("=" * 50 + "\n")

    while not game_over:
        current_fen = sf.get_fen(variant, start_fen, movelist)
        stm = get_side_to_move(variant, start_fen, movelist)

        # Redraw Board
        print(f"\n--- Move {len(movelist) + 1} | Turn: {stm.upper()} ---")
        print(get_board_rendering(engine, start_fen, movelist))
        print(f"FEN: {current_fen}")
        if movelist:
            print(f"Moves (Coordinate): {' '.join(movelist)}")
            print(f"Moves (SAN):        {' '.join(moves_san)}")

        # Check game end
        is_end, val = sf.is_immediate_game_end(variant, current_fen, [])
        if is_end:
            game_over = True
            if val == sf.VALUE_MATE:
                result_str = "1-0" if stm == 'black' else "0-1"
                print(f"\n[GAME OVER] Mate! {'White' if stm == 'black' else 'Black'} wins.")
            elif val == -sf.VALUE_MATE:
                result_str = "0-1" if stm == 'black' else "1-0"
                print(f"\n[GAME OVER] Mate! {'Black' if stm == 'black' else 'White'} wins.")
            elif val == sf.VALUE_DRAW:
                result_str = "1/2-1/2"
                print("\n[GAME OVER] Draw by adjudication.")
            else:
                result_str = "Adjudicated"
                print(f"\n[GAME OVER] Game ended. Result value: {val}")
            break

        # Check optional draw/end conditions
        is_opt_end, opt_val = sf.is_optional_game_end(variant, current_fen, [])
        if is_opt_end:
            game_over = True
            result_str = "Draw"
            print(f"\n[GAME OVER] Game ended by optional/special rule: {opt_val}")
            break

        # Check move legality
        all_legal_coords = sf.legal_moves(variant, current_fen, [])
        if not all_legal_coords:
            game_over = True
            # Check if in check to decide between checkmate and stalemate
            if sf.gives_check(variant, current_fen, []):
                result_str = "0-1" if stm == 'white' else "1-0"
                print(f"\n[GAME OVER] Checkmate! {'Black' if stm == 'white' else 'White'} wins.")
            else:
                result_str = "1/2-1/2"
                print("\n[GAME OVER] Stalemate!")
            break

        # Decide who makes the move
        is_comp_turn = self_play or (stm == 'white' and white_is_comp) or (stm == 'black' and black_is_comp)

        if is_comp_turn:
            print("\nComputer is thinking...")
            comp_move = engine.get_best_move(start_fen, movelist, movetime_ms=1000)
            if not comp_move:
                print("[ERROR] Engine returned no best move. Ending game.")
                break

            # Map best move to SAN if possible for display
            try:
                san_repr = sf.get_san(variant, current_fen, comp_move)
            except Exception:
                san_repr = comp_move

            print(f"Computer played: {san_repr} ({comp_move})")
            if self_play:
                # Add delay so the user can watch the selfplay
                time.sleep(0.8)

            movelist.append(comp_move)
            moves_san.append(san_repr)

        else:
            # Human turn
            user_input = input(f"\nEnter your move ({stm}): ").strip()
            if not user_input:
                continue

            # Check CLI Commands
            if user_input.startswith("/"):
                cmd_lower = user_input.lower()
                if cmd_lower in ["/d", "/draw", "/redraw"]:
                    continue  # The loop naturally redraws
                elif cmd_lower in ["/exit", "/quit"]:
                    confirm = input("Are you sure you want to exit this game? (y/n): ").strip().lower()
                    if confirm == 'y':
                        print("Game aborted.")
                        break
                    continue
                elif cmd_lower in ["/export"]:
                    export_game(variant, start_fen, movelist, moves_san, result_str)
                    continue
                elif cmd_lower in ["/uci"]:
                    run_direct_uci_mode(engine)
                    continue
                elif cmd_lower in ["/best", "/hint"]:
                    print("Asking engine for hint...")
                    hint = engine.get_best_move(start_fen, movelist, movetime_ms=1000)
                    if hint:
                        try:
                            hint_san = sf.get_san(variant, current_fen, hint)
                            print(f"Hint: {hint_san} (Coordinate: {hint})")
                        except Exception:
                            print(f"Hint: {hint}")
                    else:
                        print("No hint available.")
                    continue
                else:
                    print("Unknown command. Available commands: /draw, /hint, /uci, /export, /exit")
                    continue

            # Normal Move Input
            # We want to match coordinate moves (e.g. e2e4) or SAN (e.g. e4)
            chosen_move = None

            # 1. Check if the input exactly matches any legal coordinate move
            if user_input in all_legal_coords:
                chosen_move = user_input
            else:
                # 2. Translate SAN input to coordinate move by comparing with all legal moves' SAN
                matched_moves = []
                for move in all_legal_coords:
                    try:
                        san = sf.get_san(variant, current_fen, move)
                        # Case insensitive comparison of SAN moves (e.g. e4 == E4)
                        if san.lower() == user_input.lower():
                            matched_moves.append(move)
                    except Exception:
                        pass

                if len(matched_moves) == 1:
                    chosen_move = matched_moves[0]
                elif len(matched_moves) > 1:
                    print(f"Ambiguous SAN move '{user_input}'. Matched coordinates: {matched_moves}")
                    continue

            if chosen_move:
                # Successfully resolved the move
                try:
                    san_repr = sf.get_san(variant, current_fen, chosen_move)
                except Exception:
                    san_repr = chosen_move

                movelist.append(chosen_move)
                moves_san.append(san_repr)
            else:
                print(f"[ERROR] Invalid move '{user_input}'. Type a legal move (e.g., 'e4' or 'e2e4') or '/hint' for assistance.")
                print(f"Legal moves are: {', '.join(all_legal_coords)}")
                time.sleep(2)

    # Game loop exited
    if movelist:
        save_choice = input("\nWould you like to export this game? (y/n): ").strip().lower()
        if save_choice == 'y':
            export_game(variant, start_fen, movelist, moves_san, result_str)


def main():
    print_banner()

    # Initialize Engine
    try:
        engine = StockfishEngine()
    except Exception as e:
        print(f"\n[FATAL ERROR] Failed to start Stockfish subprocess: {e}")
        print("Please check that the engine is compiled at './src/stockfish' or './stockfish'.")
        sys.exit(1)

    try:
        while True:
            variant = select_variant_flow()
            play_game(engine, variant)

            # Ask if they want to play another game or quit
            again = input("\nWould you like to select another variant/game? (y/n): ").strip().lower()
            if again != 'y':
                break
    except KeyboardInterrupt:
        print("\nExiting. Thank you for playing!")
    finally:
        engine.quit()


if __name__ == "__main__":
    main()
