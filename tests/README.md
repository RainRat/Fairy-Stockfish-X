# Test suites

`tests/run.sh` is the source of truth for semantic test-suite registration.

Use `tests/build.sh` for named regression binaries. It fingerprints the build
configuration and resulting executable under `.local/build/signatures/`; a
configuration change or an unverified artifact automatically triggers an
object clean before rebuilding. Direct `make` builds still require `make clean`
when changing compiler, architecture, debug/sanitizer mode, or board-family
settings.

For the standard large-board and all-variant test binary:

```sh
tests/build.sh ARCH=x86-64-modern largeboards=yes all=yes EXE=stockfish-allvars
tests/run.sh fast src/stockfish-allvars
```

`tests/build.sh` verifies that native builds produce a runnable, non-empty
executable before accepting the artifact. The linker writes to a temporary
path and renames it atomically, and a failed rebuild leaves no executable at
the canonical path. Variant configuration validation is handled separately by
the config and variants-smoke suites.

```sh
tests/run.sh list
tests/run.sh full src/stockfish-allvars
tests/run.sh suite royal-legality src/stockfish-allvars
tests/run.sh suite movement state-transitions src/stockfish-allvars
```

Use the smallest suite matching the changed engine area:

| Changed area | Focused command |
| --- | --- |
| parser, validation, or `src/variants.ini` | `tests/run.sh suite config variants-smoke src/stockfish-allvars` |
| royal/checking/evasion/extinction/castling | `tests/run.sh suite royal-legality src/stockfish-allvars` |
| move generation, Betza, riders, hoppers, regions, topology | `tests/run.sh suite movement src/stockfish-allvars` |
| capture effects, blast, rifle, pulling, swapping | `tests/run.sh suite captures-effects state-transitions src/stockfish-allvars` |
| promotion, hands, prisons, gating, drops | `tests/run.sh suite promotion-drops state-transitions src/stockfish-allvars` |
| state, keys, do/undo, repetition | `tests/run.sh suite state-transitions src/stockfish-allvars` |
| notation, FEN, UCI, XBoard | `tests/run.sh suite notation-protocol src/stockfish-allvars` |
| search or evaluation | `tests/run.sh suite search-evaluation src/stockfish-allvars` |
| spell chess | `tests/run.sh suite spells src/stockfish-allvars` |
| Python signatures and return values | `python3 setup.py build_ext --inplace && python3 tests/python/test_pyffish_api.py` |

The search/evaluation suite includes an NNUE trace case and requires the pinned
network file. Fetch it once with `make -C src net` before running that suite.

The suite runner derives board-family requirements from the engine name. CI may
set `FSX_ENGINE_FAMILY=large` when a custom executable name needs the large-board
classification; `FSX_ALLOW_SMALL_BOARD=1` is reserved for the CI movement smoke
run that intentionally exercises a small-board debug build.

Specialized modes remain separate: `perft.sh`, `instrumented.sh`,
`regression.sh`, `regression-runner.sh`, upstream comparison programs, and
the JavaScript tests under `tests/js/`.

The benchmark compatibility wrapper accepts either a reference signature
(`tests/bench-regressions.sh [signature] [engine]`) or the stdin smoke mode
(`tests/bench-regressions.sh --stdin [engine]`).

Each semantic check is owned by one of the ten broad suites; failures identify
the suite and print a focused rerun command. Python coverage is limited to the
binding contract in `tests/python/test_pyffish_api.py`; engine-rule matrices
run through the native harness or UCI cases in the owning suite.

Successful direct suite runs are quiet and retain their logs under
`.local/build/test-run/`. Non-verbose multi-suite runs execute suites concurrently.
Set `VERBOSE=1` when streaming successful harness output is useful for debugging.
