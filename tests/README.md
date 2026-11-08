# Test suites

`tests/run.sh` is the source of truth for test-suite registration and runs the suites.

Use `tests/build.sh` to build test binaries. It tracks build options and binary signatures in `.local/build/signatures/`. If build flags change, it automatically cleans old build files before rebuilding. Direct `make` builds still require `make clean` when changing the compiler, CPU architecture, debug options, or board size settings.

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

`tests/run.sh fast` runs the registered fast suite. `tests/fast-regression.sh` is a compatibility entry point for the same run; it honors an explicitly supplied engine. The detached full regression requires all four named binaries documented above.

The config suite runs `tests/build-failure-paths.sh`, which injects dependency, linker, download, and checksum failures to verify that build failures are visible and preserve prior artifacts. `tests/board-size-options.sh` checks capacity selection under command-line flags, and `tests/lib/harness-build-profile.sh` checks harness build flags across board tiers.

The search and evaluation suite requires the NNUE network evaluation file. Download it once with `make -C src net` before running that suite; the target requires `sha256sum` or `shasum` to verify the download.

The strict test profile requires wrapper-recorded engine roles: every suite's main engine must be a large-board all-variant build; `movement` and `search-evaluation` also use a very-large-board all-variant engine, and `variants-smoke` uses large, normal, and very-large auxiliary engines. Set `FSX_TEST_PROFILE=portable` only for compatibility runs against arbitrary upstream or reduced-feature binaries. `FSX_ALLOW_SMALL_BOARD=1` explicitly permits board-role skips; those omissions appear as skip or partial-skip events. `VLB_ENGINE`, `LARGE_ENGINE`, and `NORMAL_ENGINE` override auxiliary paths in the regression runner. `FSX_ALLOW_STALE_ENGINE=1` only bypasses source-newer-than-binary checks.

Other test scripts include: `perft.sh`, `instrumented.sh`, `regression.sh`, `regression-runner.sh`, upstream comparison scripts, and the JavaScript tests in `tests/js/`. `instrumented.sh [mode] [engine]` accepts an engine path; its Syzygy probe runs only when local tablebases exist at `tests/syzygy` or `FSX_SYZYGY_DIR`. It does not download or modify test data.

The standalone `tests/perft.sh` runner reports permitted perft-set omissions
as `FSX_TEST_EVENT` partial-skip records and prints an
`FSX_TEST_SUMMARY` with the perft positions executed and any omitted sets.
Without `FSX_ALLOW_SMALL_BOARD=1`, missing required variant sets still fail.

The benchmark script accepts either a reference signature (`tests/bench-regressions.sh [signature] [engine]`) or standard input (`tests/bench-regressions.sh --stdin [engine]`).

The semantic checks are grouped into ten test suites. Each wrapped case reports its duration; failures print a `tests/run.sh case <suite> <case> [engine] [variants]` command that reruns only that case. Python tests in `tests/python/test_pyffish_api.py` verify the Python bindings directly, while chess variant rules run through native C++ test harnesses and UCI test cases.

The suite wrapper also writes tab-separated `FSX_TEST_EVENT` records
(`version`, suite, case, result, seconds, requirement, reason), where result is
`pass`, `fail`, `skip`, or `partial-skip`. `FSX_TEST_SUMMARY` reports required,
executed, skipped, partial-skipped, and failed case counts. Partial skips mean
the case passed but explicitly reported an omitted sub-check; board-family
filtering uses this path. Whole-case skips are counted as optional only when
the case is explicitly optional for the active profile. Multi-suite runs also
print a summary that totals their per-suite records, including in verbose mode.

Passing tests run quietly and save their logs in `.local/build/test-run/`. Without verbose output, multiple suites run in parallel. With `VERBOSE=1`, the runner runs suites one at a time and prints full output.

The JavaScript Makefile follows the same convention: successful builds print
only a short status, while compiler diagnostics remain visible if the build
fails. Expect-based protocol and perft checks likewise keep successful
transcripts quiet and replay failure output.
