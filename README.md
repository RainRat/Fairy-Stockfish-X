# Fairy-Stockfish-X

Fairy-Stockfish-X is an experimental fork of [Fairy-Stockfish](https://github.com/fairy-stockfish/Fairy-Stockfish) for testing new engine features and custom chess variants.

## Usage in Chess GUIs

Fairy-Stockfish-X can be used in any UCI-compatible chess GUI, such as [Cute Chess](https://cutechess.com/), Arena, or BanksiaGUI.

### Installation

1. Download or [build](DEVELOPING.md#building-from-source) the Fairy-Stockfish-X binary.
2. Add the engine to your GUI as a new UCI engine.

### Loading Variants

Fairy-Stockfish-X requires two UCI options in GUI engine settings:

- `VariantPath`: Set this to the path of the `variants.ini` file.
- `UCI_Variant`: Set this to the name of the variant, such as `shogi`, `xiangqi`, or `antichess`.

In most GUIs, you can configure these options under “Engine Settings” or “Edit Engine”.

### Move Notation

The engine uses standard coordinate notation for moves:

- Normal moves: `e2e4`, `g1f3`
- Promotions: add the piece letter at the end without an equals sign (e.g. `e7e8q` instead of `e7e8=q`)
- Drops: piece letter, `@`, and destination square, e.g. `P@b2`

## Documentation

- **[DEVELOPING.md](DEVELOPING.md):** Build instructions, CLI usage, and bindings.
- **[src/variants.ini](src/variants.ini):** Variant-specific settings and compatibility aliases.
- **[dllbinding_usage.md](dllbinding_usage.md):** C API and shared-library usage.
- **[AGENTS.md](AGENTS.md):** Development guidance for working on the codebase.

## Purpose

This project has three main goals:

1. Test new features before they move to the main project.
2. Provide a place to experiment with new ideas.
3. Support chess variants not available in the standard engine.

For the standard engine, see the [main Fairy-Stockfish repository](https://github.com/fairy-stockfish/Fairy-Stockfish).
