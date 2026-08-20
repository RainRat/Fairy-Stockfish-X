# Test suites
 
`tests/run.sh` defines and runs the test suites.

Use `tests/build.sh` to build test binaries. It tracks build options and binary signatures in `.local/build/signatures/`. If build flags change, it automatically cleans old build files before rebuilding. Direct `make` builds still require `make clean` when changing the compiler, CPU architecture, debug options, or board size settings.

For the standard large-board and all-variant test binary:

```sh
tests/build.sh ARCH=x86-64-modern largeboards=yes all=yes EXE=stockfish-allvars
tests/run.sh fast src/stockfish-allvars
```

`tests/build.sh` verifies that the build produces a working executable. The linker writes to a temporary file first and renames it on success, so a failed build never leaves a broken binary in place. Variant rules are validated separately by the `config` and `variants-smoke` test suites.

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

The search and evaluation suite requires the NNUE network evaluation file. Download it once with `make -C src net` before running that suite.

The test runner determines board-size requirements from the engine name. In CI, set `FSX_ENGINE_FAMILY=large` when testing a custom binary name that needs large-board support. Set `FSX_ALLOW_SMALL_BOARD=1` only when intentionally running movement smoke tests on a small-board build.

Other test scripts include: `perft.sh`, `instrumented.sh`, `regression.sh`, `regression-runner.sh`, upstream comparison scripts, and the JavaScript tests in `tests/js/`.

The benchmark script accepts either a reference signature (`tests/bench-regressions.sh [signature] [engine]`) or standard input (`tests/bench-regressions.sh --stdin [engine]`).

Every test belongs to one of ten test suites. If a test fails, the runner shows the failed suite and the exact command to rerun it. Python tests in `tests/python/test_pyffish_api.py` verify the Python bindings directly, while chess variant rules run through native C++ test harnesses and UCI test cases.

Passing tests run quietly and save their logs in `.local/build/test-run/`. Running multiple suites runs them in parallel. Set `VERBOSE=1` to print full test output while tests run.
