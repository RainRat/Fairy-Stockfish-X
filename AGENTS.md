# AGENTS.md — Fairy-Stockfish-X Variant Development Guide

**Fairy-Stockfish-X (FSX)** is a fork of Fairy-Stockfish for rapidly prototyping experimental chess variants. Most rules live in `src/variants.ini`; core logic in `src/` interprets those settings for search, move generation, and protocol support.

## 1) Philosophy

* Prefer **configurable settings** over one-off flags so features compose across variants.
* Keep changes **localized** to the logic they affect and maintain **backwards compatibility** with existing `.ini` files.
* Favor **performance and portability**; avoid unnecessary dependencies and hot-loop overhead.

## 2) Where to put your variant

* **Usually**: add a new entry to `src/variants.ini`. Check the “Rule definition options” and “Custom pieces” sections first.
* **Sometimes**: very popular or foundational families may live in `variant.cpp`. Don’t duplicate existing definitions.

## 3) When rules aren’t supported yet

* Break the idea into **concrete settings** (e.g., Connect-4 ⇒ `enclosingDrop=top`, `connectN=4`, not `playConnect4=true`).
* In `variant.h` → `struct Variant`: add fields for each new setting.
* In `parser.cpp`: extend `parse(Variant* v)` with `parse_attribute` for each field.
* In `position.h`: expose snake_case getters (declare under `class Position`).
* Update only the needed gameplay code:

  * Move gen / gating: `movegen.cpp::make_move_and_gating(...)`
  * Apply / unapply & legality: `position.cpp::{do_move, undo_move, legal, pseudo_legal}`
  * Betza parsing (new modifiers): `piece.cpp::from_betza(...)`
  * Adjudication: `position.cpp::{is_immediate_game_end, is_optional_game_end}`
* **Document** every new setting atop `variants.ini`; keep old keys working by reading them into the new ones with compatible defaults.

## 4) Build & run (from `src/`)

* Standard build: `make -j build ARCH=x86-64-modern`
* Large boards (>8×8, up to 10×12): `make -j build ARCH=x86-64-modern largeboards=yes`
* Heavy branching (e.g., Duck, Amazons): add `all=yes`
* Debugging: `make -j build ARCH=x86-64-modern debug=yes optimize=no`
* Launch and select variant:

  * `setoption name VariantPath value variants.ini`
  * `setoption name UCI_Variant value <your_variant>`
* UCI basics:

  * `position startpos moves e2e4 e7e5`
  * `position fen 4k3/8/8/8/8/8/p7/4K2R w K - 0 1 moves e1g1 a2a1q`
  * Drops use `@` (e.g., `position startpos moves P@b2 P@a1 P@c1`)
  * `go movetime 1000` | `go depth 20` | `d` | `quit`

## 5) Quick test automation

Create `test.txt`:

```
position startpos moves e2e4 d7d5
go movetime 100
d
quit
```

Run: `./stockfish < test.txt > output.txt`

## 6) Validation & regression (from `src/`)

* Config sanity: `./stockfish check variants.ini`
* Move-gen correctness: `../tests/perft.sh all` (or `chess`, `largeboard`)
* Protocol suite: `../tests/protocol.sh`
* Optional: `../tests/regression.sh`, `../tests/reprosearch.sh`, `./stockfish bench [variant]`

## 7) Coding style & engine notes

* C++17; 2-space indent at first function level, then +4 per nested level.
* Don’t rename existing variables unless necessary; some locals shadow globals by design.
* Bitwise ops between `Square` and `Bitboard` are overloaded in `bitboard.h`; explicit casts usually unnecessary.
* In `src/types.h` (around `piece_set()` and `PieceSet` operators), bitwise operators on `PieceSet` are overloaded as set operations, including mixed `PieceSet`/`PieceType` forms; avoid treating raw `PieceType` values as pre-shifted bit flags.
* Tuple Betza atoms `(x,y)` are now represented explicitly via `PieceInfo::tupleSteps` (`src/piece.h`) and consumed in `bitboard.cpp`; do not route long tuple leapers through `Direction`/`safe_destination` decoding.
* Extended gating FEN masks (`...|<white>/<black>`) are parsed in `Position::set`; serialization is intentionally emitted for large-board gating cases where legacy castling/gating letters are ambiguous.
* `checking = false` disables king-safety enforcement and keeps `checkersBB` empty; if a variant still needs king attacks as legal tactical threats (e.g. capturable kings), use `allowChecks = true` (`src/variant.h`) instead of re-enabling full check legality.
* `allowChecks` is not equivalent to `checking`: when `allowChecks = false`, keep the no-check king-safety path active in legality and state updates. Gating those paths on `checking_permitted()` can silently change no-check variant perft (Racing Kings is a canary).
* Forced-jump continuation followups are cached in `StateInfo::forcedJumpHasFollowup`; in hot legality/movegen paths, prefer the cached state once `forcedJumpSquare`/continuation preconditions are already established.
* For performance tuning, require swapped-order A/B runs across at least one non-chess variant (prefer `checkers` and `janggi` for jump and fairy coverage). Reject optimizations that improve one variant but regress another.
* For feature-targeted optimizations (e.g., cambodian specials, non-king castling), benchmark at least one variant that actually uses that feature and run a quick smoke search (`go depth 8`) on that variant before accepting.
* `bench <variant> ...` does not always accept `checkers` directly in this build path. For checkers performance runs, use UCI setup first (`setoption VariantPath`, `setoption UCI_Variant checkers`) and then run `bench ...` from that session.
* When benchmark outcomes are unstable, extend validation: use longer depth/time plus more swapped pairs (and optionally `taskset -c 0`) before accepting/rejecting.
* When integrating large upstream/fork PRs by cherry-pick, expect conflicts in hot files (`position.*`, `movegen.cpp`, `parser.cpp`, `test.py`). Resolve by preserving local engine invariants first (forced-jump, gating/undo consistency, custom attack paths), then layering the feature logic; run at least one variant-specific smoke test before push.
* Comments target experienced developers; don’t change copyright years.

## 8) Large/complex variants

* Use `largeboards=yes` for >8×8; boards beyond **10×12 are not supported**.
* If branching explodes, add `all=yes` to enable broader code paths and tests.

## 9) Common pitfalls

* Missing kings in FEN for king-based variants; or starting in (stale)mate.
* Assuming arbitrary castling encodings: FSX uses “king moves two squares.”
* Promotions use a trailing letter only (no `=`).
* Checkers forced jump continuation is enforced through pass semantics at UCI level (`f6f6`-style pass); pyffish convenience calls may hide that multi-ply flow if you only inspect single `legal_moves(...)` snapshots.
* Forgetting to wire new settings end-to-end: `.ini` → parser → `Variant` → getter → logic.
* Submitting without running **both** `perft` and `protocol` suites.

## 12) CI gotchas

* `./stockfish check variants.ini` on non-ALLVARS/board-limited builds can print expected warnings (missing templates, variants skipped for board limits). CI filtering should ignore those lines while still failing on real parse/syntax errors.
* `../tests/perft.sh all` includes large-board variants (e.g., shogi). Run it with a `largeboards=yes` build; otherwise it will fail at the large-board section with misleading perft mismatches.
* When switching board macro families locally (`verylargeboards=yes` ↔ `largeboards=yes` ↔ default), run `make clean` first. Reusing old objects can produce ODR/link failures and misleading diagnostics unrelated to your code change.

## 10) Research links (rules & precedent)

* Wikipedia “List of chess variants”; Chess Variants Wiki; Lichess/PyChess variant docs; BGG “Variant Chess”; Ludii library; Greenchess variants.

## 11) Before you open a PR

* Keep changes minimal and scoped; stage only what you touched.
* Verify your `.ini` parses, positions play, tests pass, and performance is sane.
* Summarize new settings in `variants.ini` comments and note any compatibility shims.
