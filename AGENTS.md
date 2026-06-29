# AGENTS.md — Fairy-Stockfish-X Guide

Fairy-Stockfish-X is a Fairy-Stockfish fork for testing experimental chess variants. Prefer `src/variants.ini` settings over one-off C++ variant hacks.

## Goals
* Prefer configurable rules that compose across variants.
* Keep changes small, local, portable, and compatible with existing `.ini` files.
* Preserve engine invariants before adding new behavior.
* Avoid extra dependencies, noisy abstractions, and hot-loop checks for impossible states.
* If behavior is unclear, prefer documented rules, then the intuitive rule, then the natural existing code path.

## Where changes go
* Variant definitions: `src/variants.ini`
* Variant fields/parsing: `variant.h`, `parser.cpp`
* Position accessors/state logic: `position.h`, `position.cpp`
* Move generation/gating: `movegen.cpp`
* Betza and movement: `piece.cpp`, `piece.h`, `bitboard.cpp`
* Tests: `tests/`, `test.py`
* Add settings end-to-end: `.ini` → parser → `Variant` field → `Position` getter → gameplay logic → tests → `variants.ini` docs.
* Keep upstream Fairy-Stockfish keys working when replacing or generalizing a setting. FSX-only experimental keys may make a clean break before the first stable release.

## Variant config compatibility
* Diagnose unknown keys, but do not reject an otherwise usable variant solely because an older engine does not recognize a newer setting.
* Never silently reinterpret an unknown key or substitute a default behavior for it.
* Reject malformed syntax and known settings with impossible, unsafe, or always-invalid values.

## Build
From `src/`:

```sh
make -j build ARCH=x86-64-modern
make -j build ARCH=x86-64-modern largeboards=yes
make -j build ARCH=x86-64-modern verylargeboards=yes
make -j build ARCH=x86-64-modern debug=yes optimize=no
make -j build COMP=mingw
```

Use `largeboards=yes` for normal large-board variants. Use `verylargeboards=yes` only beyond that matrix. When switching board macro families, run `make clean`.

For named binaries used by regression scripts:

```sh
make -j build ARCH=x86-64-modern largeboards=yes EXE=stockfish-large
make -j build ARCH=x86-64-modern verylargeboards=yes EXE=stockfish-vlb
make -j build ARCH=x86-64-modern all=yes EXE=stockfish-allvars
```

## Running the engine
Use `src/stockfish`; do not rely on a stale repo-root `./stockfish`.

```uci
setoption name VariantPath value variants.ini
setoption name UCI_Variant value <variant>
setoption name Verbosity value 0
position startpos moves e2e4 e7e5
go depth 8
d
quit
```

Drops use `@`, for example `P@b2`. Promotions use a trailing piece letter, not `=`.

## Required checks
From the repository root:

```sh
src/stockfish check src/variants.ini
bash tests/fast-regression.sh src/stockfish
tests/protocol.sh
tests/perft.sh all src/stockfish-large
```

Run large-board tests against a `largeboards=yes` binary. For Python-facing changes, run `python3 setup.py build_ext --inplace` and `python3 test.py`.

For JavaScript/wasm-facing changes, including `src/variants.ini` changes that affect `startFen`, pockets, `freeDrops`, or serialized FENs:

```sh
cd src
make -f Makefile_js build
cd ../tests/js
npm test
```

For very-large-board JavaScript/ffish work, build the ffish artifacts explicitly with very-large-board support:

```sh
cd src
make -f Makefile_js clean
make -f Makefile_js build verylargeboards=yes es6=yes
```

The generated files used by Fairyground are `tests/js/ffish.js` and `tests/js/ffish.wasm`. Fairyground can sync and wrap them without hardcoded paths:

```sh
cd /path/to/fairyground
FAIRY_WASM_REPO=/path/to/fairy-stockfish.wasm \
FAIRY_FSX_REPO=/path/to/Fairy-Stockfish-X \
npm run sync-fsx-browser-stack
npm run debug-build
```

When diagnosing browser adjudication or move-generation bugs, compare native FSX/pyffish behavior against browser ffish before changing Fairyground UI logic. If native FSX and FSX-built ffish return legal moves and `result="*"`, the bug is likely in artifact provenance or browser wiring, not adjudication policy.

For parser, movegen, legality, promotion, topology, variant-switching, or shared-state changes, run upstream checks when available:

```sh
python3 tests/upstream_reference.py src/stockfish "$UPSTREAM_ENGINE"
python3 tests/upstream_movecount_baseline.py src/stockfish "$UPSTREAM_ENGINE"
```

Only regenerate upstream baselines intentionally. Do not refresh fixtures to hide regressions.

## Full local regression
Build the named binaries first. Use the regression runner for long checks; it keeps
the full output in one log while reporting concise status and an estimate based on
recent successful runs:

```sh
tests/regression-runner.sh start src/stockfish-large
tests/regression-runner.sh status
tests/regression-runner.sh wait
```

`status` reports a check-in interval and remaining-time estimate from recent runs.
`wait` monitors the detached process and prints only the final result; use it instead
of repeatedly polling a command session. On failure it also prints the relevant log
tail. `tests/regression-runner.sh log` shows the latest log tail on demand. The runner
rejects stale named binaries before launching; rebuild the reported binary rather
than spending a full run testing old code.

The fast and full suites preserve signature-based artifacts under `.local/build`.
Do not clear that directory for a normal rerun. Use `VERBOSE=1` with
`tests/fast-regression.sh` only when successful child logs are needed; failures are
always printed in full. Special C++ harnesses rebuild their required object family
when run directly, while the fast suite prepares that family once and shares it
between harnesses.

## CI Mapping
* `Stockfish`: native engine build, perft, search, and sanitizer-style checks.
* `fairy`: variant configuration, focused regression, protocol, and variant perft checks.
* `ffishjs`: wasm build from `src/Makefile_js` plus `tests/js` `npm test`.
* `Wheels`: Python package/wheel builds; run Python binding checks for Python-facing changes.
* `Release`: packaging/release build smoke checks.

## Coding style
* C++17; follow surrounding style.
* Code is generally compact. Do not rename existing variables unless needed.
* Comments are for experienced engine developers.
* Do not update copyright years casually.
* Avoid broad wrappers, single-use helpers that hide logic, and redundant validation.
* In hot paths, do not paper over impossible states. Prefer parse-time rejection, debug assertions, or a clean crash.

## Important invariants
* `checking` and `allowChecks` are not interchangeable.
* No-check variants must keep their king-safety legality and state path active.
* Preserve FSX's split between broad royal danger and evasion-required check state.
* Use cached forced-jump continuation state when preconditions are already established.
* Spell context RAII must be nest-safe: save and restore prior context.
* Alternate repetition keys must be updated in every state transition, including null moves.
* Reserve-aware keys must keep hand and prison buckets as separate XOR terms.
* Tuple Betza atoms use `PieceInfo::tupleSteps`; do not route long tuple leapers through `Direction` decoding.
* Betza `U` is the unrestricted universal leaper. Braced `{...}` parameters configure universal hoppers; keep the two concepts and their storage paths distinct.

## Performance and pitfalls
* Benchmark affected non-chess variants when relevant; prefer `checkers` and `janggi`.
* Use swapped-order A/B runs for performance claims.
* Test feature optimizations on a variant that actually uses the feature.
* For spell-chess movegen, test both `spell-chess` and baseline chess.
* Watch for missing kings, wrong castling assumptions, large-board tests on non-largeboard builds, malformed `.ini` fallback, undocumented settings, and raw `PieceSet` bit-flag mistakes.

## PR checklist
* Config parses; changed rules are documented; old keys remain compatible where practical.
* Positions load and search; relevant perft, protocol, regression, and upstream checks pass.
* Performance-sensitive changes have before/after notes; only intended files are staged.
