# Fairy-Stockfish-X

Fairy-Stockfish-X is an experimental version of [Fairy-Stockfish](https://github.com/fairy-stockfish/Fairy-Stockfish). It is used to test new features and support unique chess variants.

## Usage in Chess GUIs

Fairy-Stockfish-X can be used in any UCI-compatible chess GUI, such as [Cute Chess](https://cutechess.com/), Arena, or BanksiaGUI.

### Installation

1. Download or [build](DEVELOPING.md#building-from-source) the Fairy-Stockfish-X binary.
2. Add the engine to your GUI as a new UCI engine.

### Loading Variants

Fairy-Stockfish-X often requires two UCI options to be set in the GUI's engine configuration:

- `VariantPath`: Set this to the path of the `variants.ini` file.
- `UCI_Variant`: Set this to the name of the variant you want to play, such as `antichess`, `shogi`, or `atomic`.

In most GUIs, these can be configured in the “Engine Settings” or “Edit Engine” dialog.

### Move Notation

The engine uses standard coordinate notation for moves.

- Normal moves use the source and destination squares, such as `e2e4` or `g1f3`.
- Promotions use a trailing piece character without an equals sign, such as `e7e8q`.
- Drops use the piece letter, an `@` symbol, and the destination square, such as `P@b2`.

## Documentation

- **[DEVELOPING.md](DEVELOPING.md):** Build instructions, CLI usage, and bindings.
- **[src/variants.ini](src/variants.ini):** Variant-specific settings and compatibility aliases.
- **[dllbinding_usage.md](dllbinding_usage.md):** C API and shared-library usage.
- **[AGENTS.md](AGENTS.md):** Development guidance for working on the codebase.

## Purpose

This project has three main goals:

1. Test new features before they move to the main project.
2. Provide a place to experiment with new ideas.
3. Support chess variants that are too unusual for the standard engine.

For standard functionality, please visit the [main Fairy-Stockfish repository](https://github.com/fairy-stockfish/Fairy-Stockfish).
