# Test Organization Plan

## Purpose

Reorganize the tests around two rules:

1. Python tests verify the `pyffish` interface. They should not be the primary tests for engine rules.
2. Shell test files represent broad, recognizable areas of the engine. A developer should be able to select the relevant suite from the code or rule they changed without knowing the history of individual regressions.

This is an organization change, not an excuse to weaken coverage or regenerate expected results. The migration should keep every existing assertion running before old entry points are removed.

## Current problems

- `src/pyffish.cpp` contains `pyffish_runCppTests()`, a collection of engine-internal rule and state tests exposed through Python only so `test.py` can call it. The tests cover promotion squares, clone targets, simulated occupancy, pawn keys, do/undo consistency, and quiet-check generation. None of these is a Python API contract.
- `test.py` also mixes public binding checks with extensive rule examples. Tests of Python types, exceptions, and serialized results belong there; tests whose only question is whether a move is legal or a result is correct generally do not.
- `tests/` currently has about 70 `.sh` files. Related behavior is spread across historical regression names. Royal handling alone is represented in files such as `royal-variant-regressions.sh`, `pseudoroyal-capture-illegal.sh`, `ep-pseudoroyal-regressions.sh`, `quiet-check-special-moves.sh`, and parts of other suites.
- `fast-regression.sh`, `local-regression.sh`, and CI each carry their own lists of test files. This makes it easy for coverage and developer guidance to diverge.
- File names often describe one bug or one variant rather than the subsystem a future change affects.

## Target structure

```text
tests/
  run.sh                         # one developer-facing selector
  suites/
    config.sh
    movement.sh
    royal-legality.sh
    captures-effects.sh
    promotion-drops.sh
    state-transitions.sh
    notation-protocol.sh
    variants-smoke.sh
    search-evaluation.sh
    spells.sh
  native/
    engine-rules.cpp             # direct Position/movegen/state tests
    test-support.hpp
  python/
    test_pyffish_api.py           # public binding contract only
  lib/
    uci.sh
    harness-build.sh
```

The exact number of broad suites can change during migration, but a new top-level `.sh` file should require a genuinely different execution environment, build family, or test mode. It should not be created merely because a regression has a new name.

Keep these specialized entry points separate because they are test modes or infrastructure rather than semantic rule categories:

- `perft.sh`
- `instrumented.sh`
- `regression.sh` for A/B benchmarking
- `regression-runner.sh` for detached full runs
- upstream comparison Python programs
- JavaScript tests under `tests/js/`

`fast-regression.sh`, `local-regression.sh`, and `allvars-regression.sh` should become compatibility wrappers around `tests/run.sh` during the transition. Once all callers are updated, retain only wrappers that still provide a useful stable public command.

## 1. Separate native rules from the Python interface

### Add a direct native rule harness

Create one native executable, initially `tests/native/engine-rules.cpp`, linked against the existing engine objects using `tests/lib/harness-build.sh`. It should initialize the engine and variant registry itself and support named groups, for example:

```text
engine-rules royal
engine-rules occupancy state
engine-rules all
```

Use a small in-tree registry of named test functions and ordinary failure messages; do not add a test-framework dependency. A shared fixture may parse inline variant definitions directly with `variants.parse_istream`, which removes the current need to load variants in Python before invoking C++ tests.

Move every case from `pyffish_runCppTests()` into this harness:

| Existing C++ case | Native group |
|---|---|
| promotion-square color handling | `promotion` |
| clone target on an empty square | `movement` |
| castling and synthetic `piece_at()` behavior | `occupancy` |
| paired-drop pawn-key recomputation | `state` |
| simulated occupancy for castling, en passant, rifle, blast, clone, paired placement, gating, and walls | `occupancy` |
| laser FEN/key/material-key consistency and do/undo | `state` |
| quiet-check generation for laser moves | `royal` |

After the native cases run in both the fast and full suites:

- remove `pyffish_runCppTests()` and the exported `run_cpp_tests` method from `src/pyffish.cpp`;
- remove `TestPyffish.test_run_cpp_tests` and its inline variant configuration from `test.py`;
- ensure the native harness is built using the same board-family object cache as the other C++ harnesses.

### Narrow the Python tests

Rename or split `test.py` into `tests/python/test_pyffish_api.py`. A test belongs there when it verifies at least one Python-facing property:

- Python argument parsing and accepted optional arguments;
- conversion between Python lists/strings/booleans and engine values;
- exception type and message for malformed input, invalid moves, or unknown variants;
- return type, tuple/list shape, ordering, and notation serialization;
- repeated calls, option changes, variant loading, or multiple positions do not leak binding state;
- a public binding returns the same observable answer as a direct engine oracle.

A test does not belong there merely because `sf.legal_moves()`, `sf.get_fen()`, or `sf.game_result()` is a convenient way to reach engine logic. Move pure rule assertions to a native group when they need internal state, or to the corresponding UCI shell suite when the public engine protocol is sufficient.

Apply that rule to the large rule-heavy sections currently named `test_legal_moves`, `test_get_fen`, `test_get_san`, `test_game_result`, `test_is_immediate_game_end`, `test_is_optional_game_end`, spell-chess tests, royal/pseudoroyal tests, connection goals, blast behavior, and variant-specific basics. Keep a small representative case for each Python method, then migrate the rule matrix behind it.

`tests/test_binding_regression.py` should be folded into the API file. `tests/test_royal_capture_no_kings.py` is a rule test and should move to `royal-legality.sh` or the native royal group.

During migration, record each moved test by its old test method or script name and its new group. Do not delete the old assertion until its new form has passed once in CI.

## 2. Consolidate shell tests by subsystem

The following is the proposed ownership map. Some large existing files will be split by test case while small files will be absorbed whole.

| Broad suite | Existing tests to absorb |
|---|---|
| `config.sh` | `parser-regressions`, `incomplete-baselines`, `explicit-custom-piece-replacements`, config-validation portions of `fast-regression-rules` and `fast-variant-regressions` |
| `movement.sh` | `movegen-regressions`, `geometry-regressions`, `eval-geometry-regressions` movement cases, `fast-regression-piece-regions`, `rider-regressions`, `non-knight-riders`, `ski-sliders`, `universal-hopper`, `separate-realms`, `wrapping-topology`, `wrapped-topology-smoke`, `gadsden-toroidal`, `test_hex_boards`, `wrapping-promotion-movegen` movement cases |
| `royal-legality.sh` | `royal-variant-regressions`, `pseudoroyal-capture-illegal`, `ep-pseudoroyal-regressions`, `quiet-check-special-moves`, `gating-check-regression`, `blast-legal-regressions`, `test_extinction`, `kings-or-lemmings`, and the royal Python rule regressions |
| `captures-effects.sh` | `capture-options-regressions`, `blast-pattern`, `jump-capture-effects`, `bycatch-undo-parity` rule cases, `rifle-chess`, `petrify-transfer`, `pulling`, `swapping`, `color-change-variants`, `capture-anything`/Benedict Python rule cases, and matching cases from `unorthodox-interactions` and `special-regressions` |
| `promotion-drops.sh` | `capture-promotion-regressions`, `drop-regressions`, `piece-promotion-gating`, `chained-piece-promotion`, `promotion-require-in-prison`, `shogi-pawn-drop-mate-split`, `castling-promoted-piece`, `wrapping-promotion-movegen` promotion cases, `largeboard-seirawan`, and drop/promotion cases from broader files |
| `state-transitions.sh` | `stateinfo-regressions`, `state-sync-key`, `in-place-transform-undo`, `bycatch-undo-parity` state cases, `clone-firstmove-split`, `multimove-rule50`, `concurrent-variant-magics`, `touched-search-regressions`, and `piece-type-bitboard-regressions.cpp` |
| `notation-protocol.sh` | `fairy-notation-regressions`, `protocol`, `xboard-regressions`, `setup-chess`, and notation/FEN round-trip cases currently in Python; keep protocol-specific `expect` support in this suite |
| `variants-smoke.sh` | `all-variants-smoke`, `new-variants-smoke`, `fast-variant-regressions` smoke cases, `mini-variant-regressions`, `vlb-regressions`, `dots-and-boxes`, `villagers`, `hindustani-baseline`, `largeboard-seirawan` smoke cases, `wrapped-connect-win`, and other tests that only establish that a named variant loads and plays |
| `search-evaluation.sh` | `bench-regressions`, `reprosearch`, `kxk-fairy-endgames`, `non8x8-endgames`, evaluation cases from `eval-geometry-regressions`, NNUE guard/export/affine tests, and search-only cases from `misc-engine-regressions` |
| `spells.sh` | `spell-freeze-regressions`, `spell-potion-movegen`, and spell-specific portions of `quiet-check-special-moves` and Python tests; always include baseline chess assertions next to spell assertions |

The remaining catch-all files (`fast-regression-rules.sh`, `special-regressions.sh`, `unorthodox-interactions.sh`, `misc-engine-regressions.sh`, and `local-regression-inline.sh`) should be emptied deliberately: classify each test block by behavior, move it, and then delete the catch-all. Individual-variant scripts such as `dots-and-boxes.sh` should likewise disappear after their smoke and rule cases have been placed in the appropriate broad suites.

Within a suite, use clearly named shell functions or sections, such as `test_pseudoroyal_capture()` or `test_stationary_castling()`. A failure must print the section and assertion name so consolidation does not make failures harder to locate. Inline variant configuration should stay adjacent to the tests that use it unless it is genuinely shared.

## 3. Provide one discoverable runner

Add `tests/run.sh` as the source of truth for suite registration. It should provide:

```text
tests/run.sh list
tests/run.sh fast [engine]
tests/run.sh full [engine]
tests/run.sh suite royal-legality [engine]
tests/run.sh suite movement state-transitions [engine]
```

The registry should store, in one place, each suite's:

- command;
- normal timeout;
- required board family (`normal`, `large`, or `very-large`);
- need for Python, `expect`, engine object files, or other build products;
- membership in `fast` and `full` profiles.

The runner should validate the supplied engine's board family before launching a suite, reuse `.local/build` artifacts, run independent fast suites in parallel, preserve full failure logs, and print the exact rerun command on failure. `fast-regression.sh` should delegate to `tests/run.sh fast`, while `local-regression.sh` and `regression-runner.sh` should use the same registry for the full profile.

Do not duplicate suite lists in GitHub Actions. CI steps should invoke profiles or named suites from `tests/run.sh`; the matrix may still choose compiler and board family.

## 4. Make test selection apparent to developers

Add the following table to `AGENTS.md` and a short `tests/README.md`. It is intentionally based on changed engine areas, not old regression filenames.

| Changed area | Minimum focused command |
|---|---|
| `variant.h`, `parser.cpp`, or validation | `tests/run.sh suite config variants-smoke src/stockfish-large` |
| `src/variants.ini` only | config check plus `variants-smoke`; also JS tests when serialized FEN, pockets, drops, or `startFen` change |
| royal, checking, evasion, extinction, or castling legality | `tests/run.sh suite royal-legality src/stockfish-large` |
| `movegen.cpp` or general legality | `tests/run.sh suite movement royal-legality src/stockfish-large` |
| Betza, riders, hoppers, regions, or topology | `tests/run.sh suite movement src/stockfish-large` |
| capture effects, blast, rifle, pulling, or swapping | `tests/run.sh suite captures-effects state-transitions src/stockfish-large` |
| promotions, hands, prisons, gating, or drops | `tests/run.sh suite promotion-drops state-transitions src/stockfish-large` |
| `StateInfo`, keys, do/undo, or repetition | `tests/run.sh suite state-transitions src/stockfish-large` |
| notation, FEN, UCI, or XBoard | `tests/run.sh suite notation-protocol src/stockfish-large` |
| search or evaluation | `tests/run.sh suite search-evaluation src/stockfish-large` |
| spell chess | `tests/run.sh suite spells src/stockfish-large` |
| `src/pyffish.cpp`, `apiutil`, or Python signatures | build the extension, then run `tests/python/test_pyffish_api.py` |
| broad/shared change before submission | `tests/run.sh fast`, followed by the detached full regression when warranted |

Tests that cross boundaries should live with the behavior most likely to regress and may be selected by more than one profile. Do not copy the same assertion into multiple shell files simply to satisfy the matrix.

## 5. Migration sequence

### Phase A: establish the runner and coverage manifest

1. Add `tests/run.sh list`, `suite`, `fast`, and `full` with the current scripts registered unchanged.
2. Generate a temporary manifest of every existing test block and its destination. Use stable section names so reviewers can verify that no assertion was lost.
3. Make existing fast/full commands delegate to the registry without changing which tests they run.
4. Capture current pass/fail status and elapsed time for the fast and full profiles.

### Phase B: remove engine tests from the Python binding

1. Add the native harness and migrate all cases from `pyffish_runCppTests()`.
2. Run the native cases against the normal and relevant large-board object families.
3. Register the harness in both fast and full profiles.
4. Remove the C++ Python method and its Python caller only after parity is demonstrated.
5. Classify the remaining Python tests, moving pure rule matrices incrementally and retaining concise API representatives.

### Phase C: consolidate shell suites

1. Start with `royal-legality.sh`, because it demonstrates the desired outcome and resolves the motivating overlap.
2. Consolidate `promotion-drops`, `captures-effects`, `movement`, and `state-transitions` next; these have the most cross-file interaction.
3. Consolidate parser/config, notation/protocol, variants, search/evaluation, and spells.
4. For each merge, run both the new suite and the old scripts once, compare the named cases and results, then remove old files and update the registry in the same change.
5. Preserve test case comments that explain a non-obvious invariant, but remove historical boilerplate and repeated UCI setup in favor of `tests/lib/uci.sh`.

### Phase D: update callers and documentation

1. Replace individual script invocations in `.github/workflows` with runner profiles or suites.
2. Update `AGENTS.md`, `tests/README.md`, and required-check commands.
3. Keep short compatibility wrappers for one release/development cycle if external users are likely to call them; wrappers should print the replacement command.
4. Remove obsolete wrappers and the temporary coverage manifest after CI and local regression have used the new layout successfully.

## Acceptance criteria

- `src/pyffish.cpp` contains production binding code only; no internal rule-test entry point is exported to Python.
- Python tests focus on the binding contract, with pure rule matrices covered natively or through UCI.
- A developer can run `tests/run.sh list` and identify a relevant suite without searching CI or `local-regression.sh`.
- Semantic shell tests are organized into roughly ten broad suites; one-bug and one-variant `.sh` files are gone.
- Fast/full membership, timeouts, prerequisites, and board families have one source of truth.
- CI and documented local commands call the same runner.
- Every pre-migration assertion has a recorded destination, and no baseline is changed merely to make the reorganization pass.
- Failures identify the suite and named case and print a focused rerun command.
- The required config check, fast regression, protocol coverage, large-board perft, Python API tests, JS tests when applicable, and upstream comparisons remain available and documented.

## Non-goals

- Do not rewrite all shell assertions into C++ solely for consistency. Use native C++ for internal invariants, UCI suites for observable engine rules, and Python only for the Python API.
- Do not combine perft, sanitizers, A/B benchmarks, upstream comparisons, or browser tests into a single opaque command.
- Do not make suite consolidation depend on a new third-party test framework.
- Do not change rule behavior, expected perft counts, or upstream fixtures as part of this reorganization.
