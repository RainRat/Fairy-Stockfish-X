# Developing Fairy-Stockfish-X

This document contains instructions for building Fairy-Stockfish-X from source, using its CLI, and working with its bindings.

## Building from Source

From the repository root, run:

```bash
make -C src -j build ARCH=x86-64-modern
```

Use `make -C src help` to see the available architecture options.

### Build Options

Add these flags to the build command when needed:

- `largeboards=yes` for variants with boards up to 12x10, such as Shogi.
- `verylargeboards=yes` for boards up to 16x16.
- `all=yes` to include unusual variants such as Game of the Amazons.

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

The search/evaluation suite also needs the pinned NNUE network; fetch it with
`make -C src net` before running that suite. See [AGENTS.md](AGENTS.md) for the
full build and testing guidance.

## CLI Usage

After building, run the engine from the repository root:

```bash
src/stockfish
```

To load the custom variant definitions and select a variant:

```uci
setoption name VariantPath value src/variants.ini
setoption name UCI_Variant value antichess
position startpos
go depth 8
d
quit
```

Useful commands include `position startpos`, `position ... moves ...`, `go depth N`, `go movetime MS`, `d` to display the position, and `help` for the complete command list.

## Python Bindings

Fairy-Stockfish-X can be used from Python through the `pyffish` library.

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
