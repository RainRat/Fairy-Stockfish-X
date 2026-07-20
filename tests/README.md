# Test suites

`tests/run.sh` is the source of truth for semantic test-suite registration.

For the normal large-board/all-variant test binary, use one of these commands:

```sh
make -C src -j4 build ARCH=x86-64-modern largeboards=yes all=yes EXE=stockfish-allvars
tests/run.sh fast src/stockfish-allvars
```

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

Specialized modes remain separate: `perft.sh`, `instrumented.sh`,
`regression.sh`, `regression-runner.sh`, upstream comparison programs, and
the JavaScript tests under `tests/js/`.

Each semantic check is owned by one of the ten broad suites; failures identify
the suite and print a focused rerun command. Python coverage is limited to the
binding contract in `tests/python/test_pyffish_api.py`; engine-rule matrices
run through the native harness or UCI cases in the owning suite.

Successful direct suite runs are quiet and retain their logs under
`.local/build/test-run/`. Set `VERBOSE=1` when streaming successful harness
output is useful for debugging.
