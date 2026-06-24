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
* Keep old keys working when replacing or generalizing a setting.

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
tests/perft.sh all
```

Run large-board tests against a `largeboards=yes` binary. For Python-facing changes, run `python3 setup.py build_ext --inplace` and `python3 test.py`.

For parser, movegen, legality, promotion, topology, variant-switching, or shared-state changes, run upstream checks when available:

```sh
python3 tests/upstream_reference.py src/stockfish "$UPSTREAM_ENGINE"
python3 tests/upstream_movecount_baseline.py src/stockfish "$UPSTREAM_ENGINE"
```

Only regenerate upstream baselines intentionally. Do not refresh fixtures to hide regressions.

## Full local regression
Prefer a detached logged run for long checks:

```sh
mkdir -p .local/logs
setsid bash -lc '/usr/bin/time -f "total elapsed %es" bash tests/local-regression.sh src/stockfish-large' > .local/logs/local-regression.$(date +%Y%m%d-%H%M%S).log 2>&1 < /dev/null &
```

Success marker: `local regression suite passed`. If missing, inspect the last `== ... ==` section and the failure above it.

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
