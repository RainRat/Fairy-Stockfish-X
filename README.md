# Fairy-Stockfish-X

Fairy-Stockfish-X is an experimental version of [Fairy-Stockfish](https://github.com/fairy-stockfish/Fairy-Stockfish). It is used to test new features and support unique chess variants.

## Usage in Chess GUIs

Fairy-Stockfish-X can be used in any UCI-compatible chess GUI (such as CuteChess, Arena, or Nibbler).

### Installation

1. Download or [build](DEVELOPING.md#building-from-source) the Fairy-Stockfish-X binary.
2. Add the engine to your GUI as a new UCI engine.

### Loading Variants

Unlike standard Stockfish, Fairy-Stockfish-X often requires two specific UCI options to be set in your GUI's engine configuration:

- **VariantPath:** Set this to the path of your `variants.ini` file.
- **UCI_Variant:** Set this to the name of the variant you want to play (e.g., `antichess`, `shogi`, `atomic`).

In most GUIs, these can be configured in the "Engine Settings" or "Edit Engine" dialog.

## Documentation

- **[DEVELOPING.md](DEVELOPING.md):** Build instructions, CLI usage, and developer information.
- **[dllbinding_usage.md](dllbinding_usage.md):** Information on using the C API / shared library.
- **[AGENTS.md](AGENTS.md):** Internal guide for developers working on the codebase.

## Purpose

This project is a fork of Fairy-Stockfish focused on:
- Testing new experimental features.
- Supporting extremely unusual or complex chess variants.
- Rapid prototyping of new chess engine ideas.

For standard functionality, please visit the [main Fairy-Stockfish repository](https://github.com/fairy-stockfish/Fairy-Stockfish).
