# AGENTS.md — Fairy-Stockfish-X Guide

Fairy-Stockfish-X is a Fairy-Stockfish fork for testing experimental chess variants. Prefer settings in `src/variants.ini` over C++ code for a single variant.

## Goals
* Prefer configurable rules that compose across variants.
* Keep changes small, local, portable, and compatible with existing `.ini` files.
* Preserve engine invariants before adding new behavior.
* Avoid extra dependencies, noisy abstractions, and hot-loop checks for impossible states.
* If behavior is unclear, use the documented rules. If none exist, use the intuitive rule. If that is unclear, follow the existing code path.

## Where changes go
* Variant definitions: `src/variants.ini`
* Variant fields/parsing: `variant.h`, `parser.cpp`
* Position accessors/state logic: `position.h`, `position.cpp`
* Move generation/gating: `movegen.cpp`
* Betza and movement: `piece.cpp`, `piece.h`, `bitboard.cpp`
* Tests: `tests/`, especially `tests/run.sh`, `tests/suites/`, and `tests/python/`
* Add each setting through every layer: `.ini` → parser → `Variant` field → `Position` getter → gameplay logic → tests → `variants.ini` docs.
* Keep upstream Fairy-Stockfish keys working when replacing or generalizing a setting. FSX-only experimental keys may make a clean break before the first stable release.

## Variant config compatibility
* Diagnose unknown keys, but do not reject an otherwise usable variant solely because an older engine does not recognize a newer setting.
* Never silently reinterpret an unknown key or substitute a default behavior for it.
* Reject malformed syntax and known settings with impossible, unsafe, or always-invalid values.

## Build
By default, compiling can be very verbose (especially under LTO). Prefer using the clean build wrapper from the root directory:

```sh
tests/build.sh ARCH=x86-64-modern
tests/build.sh ARCH=x86-64-modern largeboards=yes
tests/build.sh ARCH=x86-64-modern verylargeboards=yes
tests/build.sh ARCH=x86-64-modern hugeboards=yes all=yes EXE=stockfish-huge
tests/build.sh ARCH=x86-64-modern debug=yes optimize=no
tests/build.sh COMP=mingw
```

`tests/build.sh` checks that each native build produces a non-empty executable that can run. The linker writes to a temporary file. It replaces the previous executable only after a successful link. If linking fails, the wrapper removes the temporary file and keeps the previous executable. It also replaces `.depend` only after it scans the headers successfully. `objclean` removes object and dependency files. `objects-clean` keeps the executable for wrapper rebuilds. NNUE downloads need a checksum tool. The build fails if it cannot download and verify the network. Check `src/variants.ini` separately with the config and variants-smoke checks below.

Use `largeboards=yes` for standard large-board variants. Use `verylargeboards=yes` for boards larger than 12x10. `tests/build.sh` records the build configuration and removes old object files when flags change. Run `make clean` only when you build directly with `make -C src`.

Use `hugeboards=yes` (Huge boards / HB) for boards above 16x16, up to 22x22.
The example adds `all=yes` because the expansion coverage stresses composed
movegen (compound/hook stress reaching tens of thousands of moves on 22x22),
which needs the all-tier move lists and features; the base huge buffer already
covers drop variants at that size. It names `EXE=stockfish-huge` so the huge
binary sits alongside the other tier binaries instead of overwriting
`src/stockfish`; both expansion scripts default to that path.
Run `python3 tests/board-expansion.py src/stockfish-huge`,
`python3 tests/huge-board-placement.py src/stockfish-huge`, and
`bash tests/multiword-bitboard.sh` for expansion coverage. Keep the smaller
tiers available: huge-board search histories use substantially more memory.
The placement case reports explicit geometry skips on smaller suite engines;
run it directly against `src/stockfish-huge` to require all seven variants.

For named binaries used by regression scripts:

```sh
tests/build.sh ARCH=x86-64-modern EXE=stockfish
tests/build.sh ARCH=x86-64-modern largeboards=yes EXE=stockfish-large
tests/build.sh ARCH=x86-64-modern largeboards=yes verylargeboards=yes all=yes nnue=yes EXE=stockfish-vlb
tests/build.sh ARCH=x86-64-modern largeboards=yes all=yes EXE=stockfish-allvars
```

The `nnue=yes` on the very-large-board binary matches the `stockfish-vlb` profile
expected by `tests/regression-runner.sh`; omitting it fails the profile check.

Build these named test binaries with the wrapper so the test runner can verify their build profiles.

The very-large-board regression uses `stockfish-vlb`, found through
`VLB_ENGINE` or next to the suite engine. The VLB overlap-parity check uses a
normal-board `stockfish`, found through `NORMAL_ENGINE` or next to the suite
engine. Both checks report an error if the binary is missing. Set
`FSX_ALLOW_SMALL_BOARD=1` only when you intend to run on a small board and
expect size-related skips.
Set `FSX_ALLOW_STALE_ENGINE=1` only to skip the check for source files that
are newer than the binary. Do not use it for results you intend to keep. This
setting does not skip wrapper verification or profile checks for named
regression engines.
`FSX_ENGINE_FAMILY` selects the board-family check for custom binary names.
It does not meet the strict engine-role check, which uses wrapper records. Use
`FSX_TEST_PROFILE=portable` for intentional runs against unrecorded or
reduced-feature binaries (see `tests/README.md`).

If you prefer standard make, compile from `src/` using `make -j build ...`.

## Running the engine
Use `src/stockfish`; do not rely on a stale repo-root `./stockfish`.
The warning stays because shells resolve `./stockfish` first when run from the
root, silently testing an old binary. `tests/build.sh` now removes that stale
file automatically when it is not the requested output, but direct `make`
workflows can still leave it behind.

```uci
setoption name VariantPath value variants.ini
setoption name UCI_Variant value <variant>
setoption name Verbosity value 0
position startpos moves e2e4 e7e5
go depth 8
d
quit
```

See [README.md](README.md#move-notation) for move notation, including drops and promotions.

## Required checks
From the repository root:

```sh
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

`tests/run.sh fast` runs the registered fast suites directly; `tests/fast-regression.sh` is a compatibility wrapper for it.

Use the named binary for each broad check and report which binary you tested. A run with another binary does not show whether the documented check passes. The fast suite may also use `src/stockfish-vlb` if it exists. Rebuild it from the current sources before you run the suite.

Run large-board tests against a `largeboards=yes` binary. For Python-facing changes, run `python3 setup.py build_ext --inplace` and `python3 tests/python/test_pyffish_api.py`.

### Focused test selection

| Changed area | Minimum focused command |
| --- | --- |
| `variant.h`, `parser.cpp`, or validation | `tests/run.sh suite config variants-smoke src/stockfish-allvars` |
| `src/variants.ini` only | config plus `variants-smoke`; also JS tests when serialized FEN, pockets, drops, or `startFen` change |
| royal, checking, evasion, extinction, or castling legality | `tests/run.sh suite royal-legality src/stockfish-allvars` |
| `movegen.cpp` or general legality | `tests/run.sh suite movement royal-legality src/stockfish-allvars` |
| Betza, riders, hoppers, regions, or topology | `tests/run.sh suite movement src/stockfish-allvars` |
| capture effects, blast, rifle, pulling, or swapping | `tests/run.sh suite captures-effects state-transitions src/stockfish-allvars` |
| promotions, hands, prisons, gating, or drops | `tests/run.sh suite promotion-drops state-transitions src/stockfish-allvars` |
| `StateInfo`, keys, do/undo, or repetition | `tests/run.sh suite state-transitions src/stockfish-allvars` |
| notation, FEN, UCI, or XBoard | `tests/run.sh suite notation-protocol src/stockfish-allvars` |
| search or evaluation | `tests/run.sh suite search-evaluation src/stockfish-allvars` |
| spell chess | `tests/run.sh suite spells src/stockfish-allvars` |
| `src/pyffish.cpp`, `apiutil`, or Python signatures | build the extension, then run `tests/python/test_pyffish_api.py` |
| broad/shared change before submission | `tests/run.sh fast`, followed by the detached full regression when warranted |

For JavaScript/wasm-facing changes, including `src/variants.ini` changes that affect `startFen`, pockets, `freeDrops`, or serialized FENs:

```sh
cd src
make -f Makefile_js build
cd ../tests/js
npm test
```

For JavaScript or ffish changes that affect very large boards, build the ffish files with very-large-board support:

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

When you investigate browser game-result or move-generation bugs, compare native FSX/pyffish behavior with browser ffish before changing Fairyground UI logic. If native FSX and FSX-built ffish return legal moves and `result="*"`, the cause is likely in the built files or browser integration.

For parser, movegen, legality, promotion, topology, variant-switching, or shared-state changes, run upstream checks when available:

```sh
python3 tests/upstream_reference.py src/stockfish "$UPSTREAM_ENGINE"
python3 tests/upstream_movecount_baseline.py src/stockfish
```

The movecount command checks the committed fixture; it does not compare against a live upstream engine. Pass both engine paths with `--regenerate` only when intentionally refreshing the fixture. Do not refresh fixtures to hide regressions.

## Full local regression
Build the named binaries first. Use the regression runner for long test runs; it keeps all output in a single log file and reports estimated time remaining:

```sh
tests/regression-runner.sh start src/stockfish-allvars
tests/regression-runner.sh status
tests/regression-runner.sh wait
```

`status` reports test progress and estimated time remaining. `wait` monitors the background process and prints the final result. On failure, it prints the relevant log output. `tests/regression-runner.sh log` shows recent log output. The runner checks for up-to-date binaries before starting; rebuild the binary if prompted.

The fast and full suites keep build files under `.local/build`. Do not delete that folder for normal reruns. Tests print output only when they fail. If you run a specialized C++ harness directly, it rebuilds the objects it needs. The fast suite builds those objects once and shares them among the harnesses. Set `VERBOSE=1` with `tests/run.sh` to print all test output.

## CI Mapping
* `Stockfish`: native engine build, perft, search, and sanitizer-style checks.
* `fairy`: variant configuration, focused regression, protocol, and variant perft checks.
* `ffishjs`: WebAssembly build from `src/Makefile_js` plus `npm test` in `tests/js`.
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
* Connection adjudication is mover-only unless the variant can complete the opponent's connection (push/pull/swap/gravity, borrowed drops, blast promotion, type-shared goals, color changes) or uses count-derived goals: a new mechanic that adds, moves, or converts opponent pieces must update `connectionMoverOnly` in `Variant::conclude()`, or opponent wins will be silently missed.

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
* When asked to update an existing PR, commit and push the intended changes, refresh the PR description and test status, then verify that the remote head and PR reflect the update.
