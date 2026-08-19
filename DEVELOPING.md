# Developing Fairy-Stockfish-X

Build instructions, command-line usage, and bindings for Fairy-Stockfish-X.

## Building from Source

From the repository root, run:

```bash
make -C src -j build ARCH=x86-64-modern
```

Run `make -C src help` to see all supported CPU architectures.

### Build Options

Add these flags when needed:

- `largeboards=yes` for boards up to 12x10 (e.g. Shogi, Xiangqi).
- `verylargeboards=yes` for boards up to 16x16.
- `all=yes` to include all remaining variants.

For example:

```bash
make -C src -j build ARCH=x86-64-modern largeboards=yes
```

### Required Checks

The standard local checks are:

```bash
tests/build.sh ARCH=x86-64-modern largeboards=yes all=yes EXE=stockfish-allvars
src/stockfish-allvars check src/variants.ini
bash tests/fast-regression.sh src/stockfish-allvars
tests/protocol.sh src/stockfish-allvars
tests/perft.sh all src/stockfish-allvars
```

The search and evaluation test suite requires the NNUE evaluation file. Download it with `make -C src net` before running those tests. See [AGENTS.md](AGENTS.md) for full testing instructions.

## CLI Usage

After building, run the engine from the repository root:

```bash
src/stockfish
```

To load custom variant definitions and select a variant:

```uci
setoption name VariantPath value src/variants.ini
setoption name UCI_Variant value antichess
position startpos
go depth 8
d
quit
```

Common engine commands:
- `position startpos` — set up the starting position
- `position ... moves ...` — set up a position and play moves
- `go depth N` — search to depth N
- `go movetime MS` — search for MS milliseconds
- `d` — print an ASCII board of the current position
- `help` — show the full command list

## Python Bindings

Fairy-Stockfish-X includes Python bindings through the `pyffish` library.

### Prerequisites
 
- Python 3.x
- `setuptools` (`pip install setuptools`)
 
### Building the Extension

From the project root, run:

```bash
python3 setup.py build_ext --inplace
```

Once built, you can generate legal moves and parse FENs:

```python
import pyffish as sf

variant = "chess"
fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
moves = sf.legal_moves(variant, fen, [])
print(f"Legal moves: {moves}")

new_fen = sf.get_fen(variant, fen, ["e2e4"])
print(f"New FEN: {new_fen}")
```

## C API

Fairy-Stockfish-X can be built as a shared library with a C API. See [dllbinding_usage.md](dllbinding_usage.md) for details.
