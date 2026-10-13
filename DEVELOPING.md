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
- `hugeboards=yes` for Huge boards (HB), up to 22x22 (files A–V, ranks 1–22).
- `all=yes` to include all remaining variants.

For example:

```bash
make -C src -j build ARCH=x86-64-modern largeboards=yes
```

`boardbits=64|128|256|512` selects one compile-time capacity. The existing
Boolean options still select 64, 128, or 256 bits when `boardbits` is omitted.
`hugeboards=yes` selects 512 bits; `boardbits=512` remains supported.
Explicit Boolean options must agree with the selected capacity. The 256- and
512-bit builds use the same multiword operations and generic rider geometry;
their packed moves occupy 64 bits. Magic tables and PEXT are disabled for both.
The 512-bit tier also uses 32-bit evaluation score lanes so dense positions
do not overflow the ordinary 16-bit lanes.
Keep the 256-bit tier for existing variants: the 512-bit executable has larger
geometry and search-history tables even when it searches a smaller board.

Swapped-order GCC 11.4 measurements against the pre-change 256-bit build found
Chu Shogi +0.3%, Dai Shogi -2.2%, and 16x16 Hex -4.2% in nodes per second,
with matching node totals. This PR retains the shared arithmetic at that
measured cost; shift-unrolling and bit-clearing experiments slowed other games.
Recovering that cost requires further measured optimization.

Make builds select capacity through these options, rather than legacy `-D`
flags in `CXXFLAGS`. Direct compiler builds still accept `-DLARGEBOARDS`
or `-DVERY_LARGE_BOARDS` when `BOARD_BITS` is absent. Compatibility macros
that require a larger board than an explicit `BOARD_BITS` fail at compile time.

```bash
tests/build.sh ARCH=x86-64-modern hugeboards=yes all=yes EXE=stockfish-huge
python3 tests/board-expansion.py src/stockfish-huge
python3 tests/huge-board-placement.py src/stockfish-huge
bash tests/board-size-options.sh
bash tests/multiword-bitboard.sh
```

The selector and `hugeboards=yes` alias also work with `src/Makefile_js` and
`src/Makefile_dll`. The engine banner includes `HB 512`; recorded build profiles
keep the numeric capacity. For the
Python extension, set `CFLAGS="-O3 -DNDEBUG -DBOARD_BITS=512"` and
`CXXFLAGS="-O3 -DNDEBUG -DBOARD_BITS=512"`, then run
`python3 setup.py build_ext --inplace --force`. Rebuild when switching capacity;
object files and bindings from different tiers are incompatible.

The makefiles append required configuration flags even when `CXXFLAGS` is
supplied on the command line. Use `optimize=no` to avoid the makefile's
optimization flags when supplying your own. DLL objects are stored separately
per capacity under `.local/build/dll`; compiler-flag and header changes rebuild
them automatically. A link stamp also tracks the selected cached objects, so
size switches relink the library and identical builds do no compiling or linking.
`bash tests/dll-board-size.sh` checks size switches, compiler flags, header
dependencies, and unchanged builds.

`bash tests/huge-movegen.sh` runs independent combined-route, free-drop, and
arrow-wall tests against both move generation and move picking under ASan and
UBSan. It includes cases above `MAX_MOVES` without increasing the existing
overflow capacities. The Windows MSVC workflow checks both multiword widths,
table and hardware popcount, and engine builds.
CI runs the full ASan/UBSan stress build with GCC. Both GCC and Clang run the
Huge debug engine checks and the independent UBSan arithmetic tests.

A 512-bit build currently uses about 74 MiB for fixed geometry tables and
roughly 673 MiB resident memory at native startup without embedded NNUE, with
one thread and a 16 MiB hash table. Most of the per-thread allocation is
continuation history (about 458 MiB per thread). This allocation also applies
when a huge-board executable plays a smaller variant; use a smaller tier for
those games. Budget roughly 548 MiB per additional search thread for
histories and pawn/material caches, plus search buffers and stack space.
WebAssembly starts with 128 MiB and can grow to 1 GiB, with an 8 MiB stack for
SAN and rule probes. Boards above
22x22 and complete rules for historical large shogi games require separate work.

The shipped huge-board placement variants are `gomoku-19x19`, `hex-19x19`,
`hex-22x22`, `go19`, `capture-go19`, `nogo19`, and `suicide-go19`.
Gomoku uses freestyle five-in-a-row rules. Hex inherits the existing
`openingSwapDrop` surrogate. Ordinary and suicide Go retain the existing
liberty/repetition rules as experimental rule-testing setups and draw after
two passes; territory/area scoring,
dead-group adjudication, and komi require further work. Large shogi additions
are deferred to a possible future PR.

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

Native UCI `position` commands are strict: malformed syntax, invalid
variant-aware FEN, or any illegal move prints an `info string` diagnostic and
exits with failure. The engine never searches a valid prefix of a rejected
move list. The Python API raises `ValueError` for invalid FEN and invalid
moves. JavaScript board creation and `setFen()` throw on invalid FEN; the C
binding reports failure from board creation or `fsf_set_fen()`. Native XBoard
rejects invalid `setboard` FEN with an error and retains its previous position;
an illegal XBoard move is reported and skipped.

Strict FEN checks use the selected variant's board, piece, pocket, promotion,
castling, and rule-specific fields. They do not impose orthodox chess rules on
variants.

Each interface still has its own contract for other malformed commands:
- **`go searchmoves`**: tokens that match no legal move select nothing. A
  list with no legal match searches no moves (`bestmove (none)`).
- **`go` numeric limits** (`depth`, `nodes`, `mate`, `perft`, `movetime`,
  `movestogo`, `wtime`/`btime`, `winc`/`binc`, UCCI/USI time fields):
  malformed or negative values reject just that command with an
  `info string error: ...` diagnostic; the engine stays alive and answers
  the next command. This mirrors upstream's fail-fast spirit without
  upstream's abort, which would lose GUI games.
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

The Python binding raises `ValueError` for invalid FEN and move lists. The
JavaScript `Board.push()` contract follows upstream ffish.js: it returns
`false` and leaves the board unchanged. The native `go searchmoves` path also
expects valid move strings.

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
