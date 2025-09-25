# Fairy-Stockfish Variant Development Guide

## Project Overview

Fairy-Stockfish-X provides a framework for experimenting with chess variants on top of the Fairy-Stockfish engine. The repository centers on `variants.ini` for defining rules and custom pieces, while core logic in the `src` directory interprets those settings and drives search, move generation, and protocol support.

## Adding a Variant

- Most variants only require a new entry in `variants.ini`. Review the **Rule definition options** and **Custom pieces** tables in that file to see whether existing attributes already express your rules.
- Popular or foundational variants occasionally live directly in `variant.cpp`. Check there before introducing duplicate definitions.

## When Rules Aren't Supported

1. Break your idea into concrete settings (e.g., `enclosingDrop = top`, `connectN = 4`) so the engine stays flexible instead of adding one-off flags.
2. Extend `struct Variant` in `variant.h` with any new fields, parse them in `parser.cpp`, and expose a matching getter in `position.h`.
3. Update gameplay logic only where the new rule applies—typically move generation (`movegen.cpp`) or move execution and legality checks in `position.cpp`. Add adjudication or Betza parsing logic only if the rule demands it.
4. Document every new setting in the comment block at the top of `variants.ini`, maintain backwards compatibility with existing configuration keys, and favor performance-friendly implementations.

## Testing Your Variant

- Build from the `src` directory with `make`. Use the provided options (`ARCH=`, `largeboards=`, `all=`, `debug=`) as needed for your variant.
- Launch the engine, set `VariantPath` and `UCI_Variant`, then drive play with standard UCI commands such as `position`, `go movetime`, or scripted input redirection.
- Run `stockfish check variants.ini` whenever you edit the configuration file to verify its integrity.
- Ensure your test FENs are legal and include any required kings or drops so the engine can reach the scenarios you expect.

## Run Tests After Changes

After every code modification, execute the core regression suites from the `src` directory to confirm no variant or protocol regressions were introduced:

- `../tests/perft.sh all`
- `../tests/protocol.sh`

Only submit changes once these tests pass or you have investigated and understood any failures.
