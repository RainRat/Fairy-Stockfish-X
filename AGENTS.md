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
* Comments target experienced developers; don’t change copyright years.

## 8) Large/complex variants

* Use `largeboards=yes` for >8×8; boards beyond **10×12 are not supported**.
* If branching explodes, add `all=yes` to enable broader code paths and tests.

## 9) Common pitfalls

* Missing kings in FEN for king-based variants; or starting in (stale)mate.
* Assuming arbitrary castling encodings: FSX uses “king moves two squares.”
* Promotions use a trailing letter only (no `=`).
* Forgetting to wire new settings end-to-end: `.ini` → parser → `Variant` → getter → logic.
* Submitting without running **both** `perft` and `protocol` suites.

## 10) Research links (rules & precedent)

* Wikipedia “List of chess variants”; Chess Variants Wiki; Lichess/PyChess variant docs; BGG “Variant Chess”; Ludii library; Greenchess variants.

## 11) Before you open a PR

* Keep changes minimal and scoped; stage only what you touched.
* Verify your `.ini` parses, positions play, tests pass, and performance is sane.
* Summarize new settings in `variants.ini` comments and note any compatibility shims.
