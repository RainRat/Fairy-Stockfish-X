# Developing Fairy-Stockfish-X

Build instructions, command-line usage, and bindings for Fairy-Stockfish-X.

## Building from Source

From the repository root, use the build wrapper. It records a profile for the
executable and shared objects:

```bash
tests/build.sh ARCH=x86-64-modern
```

Run `make -C src help` to list the supported CPU architectures.

### Build Options

Use these flags when you need them:

- `largeboards=yes` for boards up to 12x10 (e.g. Shogi, Xiangqi).
- `verylargeboards=yes` for boards up to 16x16.
- `hugeboards=yes` for Huge boards (HB), up to 22x22 (files A–V, ranks 1–22).
- `all=yes` to include all remaining variants.

For example:

```bash
tests/build.sh ARCH=x86-64-modern largeboards=yes
```

The default build supports boards up to 8x8. `largeboards=yes` supports 12x10,
`verylargeboards=yes` supports 16x16, and `hugeboards=yes` supports 22x22.
Boolean options for board size must agree. Very-large and huge builds
share multiword operations and generic rider geometry. Both disable magic
tables and PEXT. Huge builds use wider evaluation scores for dense
positions. Composed movegen on 22x22 can need tens of thousands of move
entries, so the huge test build uses `all=yes` for
the larger move lists; the base huge buffer already covers drop variants at
that size.
Keep the very-large tier for existing variants. The huge executable uses more
memory for geometry and search-history tables, even when it searches a smaller board.

GCC 11.4 measurements, with the test order swapped, compared this build with the
earlier very-large build. They found
Chu Shogi +0.3%, Dai Shogi -2.2%, and 16x16 Hex -4.2% in nodes per second,
with matching node totals. This PR retains the shared arithmetic at that
measured cost; shift-unrolling and bit-clearing experiments slowed other games.
Recovering that cost requires further measured optimization.

Use the Boolean options to select capacity in make builds. Direct compiler builds
also accept `-DLARGEBOARDS`, `-DVERY_LARGE_BOARDS`, or `-DHUGEBOARDS`.
The compiler rejects conflicting capacity macros.

```bash
tests/build.sh ARCH=x86-64-modern hugeboards=yes all=yes EXE=stockfish-huge
python3 tests/board-expansion.py src/stockfish-huge
python3 tests/huge-board-placement.py src/stockfish-huge
bash tests/board-size-options.sh
bash tests/multiword-bitboard.sh
```

The Boolean options also work with `src/Makefile_js` and
`src/Makefile_dll`. The engine banner includes `HB`. Recorded build profiles
use `board=huge`, like `normal`, `large`, and `very-large`. For the
Python extension, set `CFLAGS="-O3 -DNDEBUG -DHUGEBOARDS"` and
`CXXFLAGS="-O3 -DNDEBUG -DHUGEBOARDS"`, then run
`python3 setup.py build_ext --inplace --force`. Rebuild when switching capacity;
object files and bindings from different tiers do not work together.

The selected `BOARD_BITS` capacity stays active when you set `CXXFLAGS` on the
make command line. Use `EXTRACXXFLAGS` to add compiler flags and keep the
makefile's standard language and safety flags enabled. DLL objects are stored in
separate folders for each capacity under `.local/build/dll`. Compiler-flag and
header changes rebuild them. A link stamp tracks the cached objects, so a
capacity change relinks the library and an identical build skips compilation
and linking.
`bash tests/dll-board-size.sh` checks size switches, compiler flags, header
dependencies, and unchanged builds.

`bash tests/huge-movegen.sh` runs combined-route, free-drop, and arrow-wall
tests under ASan and UBSan. It checks both move generation and move picking.
Some cases exceed `MAX_MOVES`; the test does not increase the existing
overflow capacities. The Windows MSVC workflow checks both multiword widths,
table and hardware popcount, and engine builds.
CI runs the full ASan/UBSan stress build with GCC. Both GCC and Clang run the
Huge debug engine checks and the independent UBSan arithmetic tests.

A huge-board build uses about 74 MiB for fixed geometry tables and
roughly 673 MiB resident memory at native startup without embedded NNUE, with
one thread and a 16 MiB hash table. Most of the per-thread allocation is
continuation history (about 458 MiB per thread). This allocation also applies
when a huge-board executable plays a smaller variant; use a smaller tier for
those games. Budget roughly 548 MiB per additional search thread for
histories and pawn/material caches, plus search buffers and stack space.
WebAssembly starts with 128 MiB and can grow to 1 GiB, with an 8 MiB stack for
SAN and rule probes. Boards above
22x22 and complete rules for historical large shogi games require separate work.

The included huge-board placement variants are `gomoku-19x19`, `hex-19x19`,
`hex-22x22`, `go19`, `capture-go19`, `nogo19`, and `suicide-go19`.
Gomoku uses freestyle five-in-a-row rules. Hex inherits the existing
`openingSwapDrop` surrogate. Ordinary and suicide Go retain the existing
liberty/repetition rules as experimental rule-testing setups and draw after
two passes; territory/area scoring,
dead-group adjudication, and komi require further work. Large shogi additions
are deferred to a possible future PR.

### Required Checks

Run these standard checks locally:

```bash
tests/build.sh ARCH=x86-64-modern largeboards=yes all=yes EXE=stockfish-allvars
src/stockfish-allvars check src/variants.ini
bash tests/fast-regression.sh src/stockfish-allvars
tests/protocol.sh src/stockfish-allvars
tests/perft.sh all src/stockfish-allvars
```

Build the named binaries with the wrapper before the detached full regression:

```sh
tests/build.sh ARCH=x86-64-modern EXE=stockfish
tests/build.sh ARCH=x86-64-modern largeboards=yes EXE=stockfish-large
tests/build.sh ARCH=x86-64-modern largeboards=yes verylargeboards=yes all=yes nnue=yes EXE=stockfish-vlb
tests/build.sh ARCH=x86-64-modern largeboards=yes all=yes EXE=stockfish-allvars
```

`tests/run.sh fast` runs the registered fast suites. `tests/fast-regression.sh` is a compatibility wrapper for the same command.

The search and evaluation test suite requires the NNUE evaluation file. Download it with `make -C src net` before running those tests. See [AGENTS.md](AGENTS.md) for full testing instructions.

## CLI Usage

After you build the engine, run it from the repository root:

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

Public FEN entry points check input against the selected variant before loading
it. Native UCI rejects an invalid `position` command with an `info string`
diagnostic, then exits with an error. Like official Stockfish, it treats a bad
position command as a fatal protocol error. It does not search the valid part
of an invalid move list.
Python raises `ValueError` for invalid FEN and moves. JavaScript board creation
and `setFen()` throw on invalid FEN. The C API returns `nullptr` from
`fsf_new_board()` and `false` from `fsf_set_fen()` for invalid input. Native
XBoard rejects invalid `setboard` FEN and retains its previous position; an
illegal XBoard move is reported and skipped. Native XBoard likewise reports
`Error (bad holding)` and keeps its position when a holding update cannot be
represented (no pocket section).

Strict FEN checks use the selected variant's board, piece, pocket, promotion,
castling, and rule-specific fields. They do not impose orthodox chess rules on
variants.

Thai SAN/LAN uses Thai coordinate symbols for the first 12 files and ranks.
Coordinates beyond those tables use algebraic file letters and decimal ranks.

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
  raises `ValueError`; the helper returns no partially built position.
- **JS (`ffish.js`)**: per-call choice. `push` returns `false` per move,
  `pushMoves` stops at the first failure keeping the prefix, and
  `variationSan` rolls everything back and returns `""`.
- **DLL C API**: the legacy `fsf_push_moves` functions stop at the first bad
  move and keep the prefix, but return no status. `fsf_try_push_moves` and
  `fsf_try_push_san_moves` report success and the number applied through an
  output parameter; a failed batch still keeps its legal prefix. The legacy
  `fsf_is_capture` returns false for both a quiet move and invalid input;
  `fsf_try_is_capture` distinguishes them. Legacy `fsf_set_option_*` calls
  ignore unknown names; `fsf_try_set_option_*` reports failure.
- **Low-level FEN parsing**: `Position::set()` assumes its caller has already
  validated the FEN. Its parser is best-effort for malformed fields and is not
  a public input-validation API. Public UCI, Python, JavaScript, and C entry
  points validate before loading. `validate_fen` and `validate_position` are
  also available for checking input explicitly; gating-mask suffixes are
  strictly validated there.

The DLL C API accepts a null `fsf_board` handle safely: string getters return
an allocated empty string, numeric getters return zero, boolean getters return
false, mutators do nothing (or return false), and `fsf_free_board(nullptr)` is
safe. A failed `fsf_new_board()` returns `nullptr`; check it before calling
board operations. Read-only global getters and validators initialize the
engine on first use, so calling `fsf_init()` explicitly is optional.

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

The bindings share engine rules but keep surface-specific call and error
contracts. Invalid FEN raises `ValueError` in Python, throws in JavaScript
constructors and `setFen()`, and returns `nullptr`/`false` in the DLL. Python
helpers that accept a move list reject the request with `ValueError` and return
no position. JS `pushMoves` and legacy DLL bulk-push stop at the first invalid
move and keep the valid prefix. The DLL's `fsf_try_push_*` functions expose
success and the applied count without changing that prefix behavior.

Single-move failures also differ by surface: Python raises `ValueError`, JS
`Board.push()` returns `false`, and DLL `fsf_push()` returns `false`. For C
callers, `fsf_try_is_capture()` distinguishes malformed moves from valid quiet
moves; legacy `fsf_is_capture()` returns false for either. Use
`fsf_try_set_option_*()` to detect an unknown option; legacy setters keep their
void signatures and ignore unknown names.

Argument order follows the existing public APIs. Python `validate_fen(fen,
variant)` and `get_fog_fen(fen, variant)` are FEN-first, while the other
position helpers take `variant` first. JS `validateFen(fen, variant)` and
`getFogFen(fen, variant)` are also FEN-first. DLL validators take
`(fen, variant, is960)`; board construction takes `(variant, fen, is960)`.

`pyffish.game_result()` is an FSX extension of upstream's older terminal
position helper. In addition to variant endings and checkmate/stalemate, FSX
considers mutual insufficient material and optional draw conditions, and
normalizes public mate scores. Python includes optional draw conditions in
`game_result()`; JavaScript and DLL `result()` default to no optional claim, and
`result(true)` includes one. Across all three result APIs, forced endings and
insufficient material precede no-legal-move results, which precede optional
draw claims. `variantInfo()` and DLL `fsf_variant_info()` expose the shared
schema: Python raises `ValueError` for an unknown variant, while JS and DLL
return an empty string.

`variantInfo()` returns resolved configuration JSON with `schemaVersion: 2`.
This is a pre-release FSX format, not an upstream pyffish/ffish.js contract.
Schema 2 removes the obsolete `promotion.pieceTypesByFile` and
`promotion.pieceTypesByRank` fields after promotion regions were generalized;
consumers must use the current `promotion.pieceTypes` and
`promotion.regionsByPiece` fields instead. No compatibility alias for the
removed fields is provided.

## C API

Fairy-Stockfish-X can be built as a shared library with a C API. See [dllbinding_usage.md](dllbinding_usage.md) for details.
