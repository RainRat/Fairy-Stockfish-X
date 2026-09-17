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

## Invalid input handling

Each interface has its own contract for malformed FEN and move lists. This is
deliberate — match the surface you are calling, and do not "unify" them
without a GUI-compatibility review:

- **Native UCI `position ... moves`**: fail-open truncation. The valid prefix
  is applied; the first unparsable token and everything after it are silently
  ignored. This matches upstream Fairy-Stockfish and keeps games going when a
  GUI sends a notation the engine does not understand (e.g. a 960-unaware
  `e1h1`). There is no error channel on the `position` command.
- **Native XBoard**: per-move `Illegal move: <token>` diagnostics; the bad
  move is skipped.
- **`go searchmoves`**: tokens that match no legal move select nothing. A
  list with no legal match searches no moves (`bestmove (none)`).
- **Python (`pyffish`)**: whole-request rejection. A bad move in the list
  raises `ValueError` and applies nothing.
- **JS (`ffish.js`)**: per-call choice. `push` returns `false` per move,
  `pushMoves` stops at the first failure keeping the prefix, and
  `variationSan` rolls everything back and returns `""`.
- **FEN loading** (`position fen`, `set_fen`, board constructors) is
  best-effort everywhere: unknown pieces, off-board squares, and bad flags
  are skipped, not rejected. Use the opt-in `validate_fen` /
  `validate_position` APIs (all three bindings) before loading untrusted
  input, and note that gating-mask suffixes are strictly validated there.

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

### Binding and metadata compatibility

The Python binding follows upstream pyffish's move-list contract: an invalid
move raises `ValueError`. The JavaScript `Board.push()` contract follows
upstream ffish.js: it returns `false` and leaves the board unchanged. These
are separate from the native UCI contract, where `position ... moves` applies
the valid prefix and stops at the first invalid move; callers should validate
input before sending malformed UCI commands. The native `go searchmoves` path
also expects valid move strings.

`pyffish.game_result()` is an FSX extension of upstream's older terminal
position helper. In addition to variant endings and checkmate/stalemate, FSX
considers mutual insufficient material and optional draw conditions, and
normalizes public mate scores. The JavaScript `Board.result()` API keeps the
upstream default: `result()` and `result(false)` do not claim optional draws;
`result(true)` does. `variantInfo()` is FSX-only: Python raises `ValueError`
for an unknown variant, while JavaScript returns an empty string.

`variantInfo()` returns resolved configuration JSON with `schemaVersion: 2`.
This is a pre-release FSX format, not an upstream pyffish/ffish.js contract.
Schema 2 removes the obsolete `promotion.pieceTypesByFile` and
`promotion.pieceTypesByRank` fields after promotion regions were generalized;
consumers must use the current `promotion.pieceTypes` and
`promotion.regionsByPiece` fields instead. No compatibility alias for the
removed fields is provided.

## C API

Fairy-Stockfish-X can be built as a shared library with a C API. See [dllbinding_usage.md](dllbinding_usage.md) for details.
