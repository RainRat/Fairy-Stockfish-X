# Test suites

`tests/run.sh` registers and runs the test suites. Treat it as the source of truth.

Use `tests/build.sh` to build test binaries. It records build options and binary signatures in `.local/build/signatures/`. If the build flags change, it removes old build files before rebuilding. If you use `make` directly, run `make clean` when you change the compiler, CPU architecture, debug options, or board size settings.

Named executables are written to `src/<EXE>` and share the same `src/*.o`
files. The wrapper tracks the object configuration separately from each
binary, cleans shared objects when that configuration changes, and checks the
finished binary's advertised board family. Use it when building test binaries
so the test runner can verify that the selected executable has the expected
profile.

Build the named test binaries with the wrapper so each has a recorded profile:

```sh
tests/build.sh ARCH=x86-64-modern EXE=stockfish
tests/build.sh ARCH=x86-64-modern largeboards=yes EXE=stockfish-large
tests/build.sh ARCH=x86-64-modern largeboards=yes verylargeboards=yes all=yes nnue=yes EXE=stockfish-vlb
tests/build.sh ARCH=x86-64-modern largeboards=yes all=yes EXE=stockfish-allvars
```

`tests/run.sh fast src/stockfish-allvars` runs the registered fast suites. `tests/fast-regression.sh src/stockfish-allvars` is a compatibility wrapper for that command.

`tests/build.sh` verifies that the build produces a working executable. See [AGENTS.md](../AGENTS.md) for how failed native builds preserve the prior executable and dependency file. Variant rules are validated separately by the `config` and `variants-smoke` test suites.

```sh
tests/run.sh list
tests/run.sh full src/stockfish-allvars
tests/run.sh suite royal-legality src/stockfish-allvars
tests/run.sh suite movement state-transitions src/stockfish-allvars
tests/run.sh case royal-legality native-adjudication src/stockfish-allvars
```

Choose the smallest suite that covers the engine area you changed:

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

`tests/run.sh fast` runs the registered fast suite. `tests/fast-regression.sh` runs the same suite for compatibility and uses an engine path if you provide one. The detached full regression needs all four named binaries listed above.

The config suite runs `tests/build-failure-paths.sh`. It creates dependency, linker, download, and checksum failures to check that build errors are visible and old files remain intact. `tests/board-size-options.sh` checks capacity selection from command-line flags. `tests/lib/harness-build-profile.sh` checks harness build flags across board tiers.

The search and evaluation suite requires the NNUE network evaluation file. Download it once with `make -C src net` before running that suite; the target requires `sha256sum` or `shasum` to verify the download.

The strict test profile requires the build wrapper to record each engine's role. Every suite's main engine must support large boards and all variants. The `movement` and `search-evaluation` suites also use an engine that supports very large boards and all variants. The `variants-smoke` suite uses auxiliary engines for large, normal, and very large boards. Set `FSX_TEST_PROFILE=portable` only when you need compatibility with an upstream or reduced-feature binary. Set `FSX_ALLOW_SMALL_BOARD=1` to allow tests to skip checks that need larger boards. These omissions appear as skip or partial-skip events. Use `VLB_ENGINE`, `LARGE_ENGINE`, and `NORMAL_ENGINE` to set auxiliary engine paths in the regression runner. `FSX_ALLOW_STALE_ENGINE=1` skips only the source-newer-than-binary check.

Other test scripts include `perft.sh`, `instrumented.sh`, `regression.sh`, `regression-runner.sh`, upstream comparison scripts, and the JavaScript tests in `tests/js/`. `regression.sh` limits UCI variant discovery and bench probes with `FSX_REGRESSION_PROBE_TIMEOUT` (default: `15m`). Run `bash tests/regression-discovery.sh` to check timeout and auto-discovery behavior. `instrumented.sh [mode] [engine]` accepts an engine path. Its Syzygy probe runs only if local tablebases exist in `tests/syzygy` or at `FSX_SYZYGY_DIR`. The script does not download or change test data.

The standalone `tests/perft.sh` runner reports allowed perft-set omissions as
`FSX_TEST_EVENT` partial-skip records. It prints an `FSX_TEST_SUMMARY` with the
perft positions it ran and any sets it omitted. Missing required variant sets
still cause failure unless you set `FSX_ALLOW_SMALL_BOARD=1`.

The benchmark script accepts either a reference signature (`tests/bench-regressions.sh [signature] [engine]`) or standard input (`tests/bench-regressions.sh --stdin [engine]`).

The semantic checks are grouped into ten suites. Each case reports how long it took. If a case fails, the output includes a `tests/run.sh case <suite> <case> [engine] [variants]` command to rerun only that case. `tests/python/test_pyffish_api.py` checks the Python bindings. Native C++ test harnesses and UCI test cases check chess variant rules.

The suite wrapper writes `FSX_TEST_EVENT` records as tab-separated fields:
`version`, suite, case, result, seconds, requirement, and reason. The result is
`pass`, `fail`, `skip`, or `partial-skip`. `FSX_TEST_SUMMARY` reports the counts
of required, run, skipped, partly skipped, and failed cases. A partial skip
means the case passed but reported a skipped sub-check. Board-family filtering
uses this result. The runner counts a whole-case skip as optional only if the
active profile marks the case as optional. Runs with several suites also print
one summary with the totals from each suite, including in verbose mode.

Passing tests run quietly and save logs in `.local/build/test-run/`. By default,
the runner runs several suites at the same time. Set `VERBOSE=1` to run suites
one at a time and print all output.

The JavaScript Makefile also prints a short status when a build succeeds and
shows compiler messages when it fails. Expect-based protocol and perft checks
keep successful output quiet and replay it when a check fails.
