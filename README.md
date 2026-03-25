# Fairy-Stockfish-X

## Purpose

Fairy-Stockfish-X is an experimental branch of the Fairy-Stockfish project with the following goals:

1. To test and combine various feature suggestions that are not yet ready for the main Fairy-Stockfish codebase.
2. To provide a platform for iterating on these experimental features.
3. To serve as a permanent home for chess variants that are considered too unconventional for the mainline code.

## Key Experimental Features

Compared to the mainline Fairy-Stockfish, this branch includes several experimental extensions:

- **Expanded Betza Support**: Full support for `(x,y)` tuple leapers and improved handling of complex riders.
- **Forced Jump Mechanics**: Configurable forced-jump continuations (`forcedJumpContinuation`), commonly used in Checkers variants.
- **Walling Rules**: Support for dynamic board modifications, including Amazon-style arrows (`arrow`), mobile blocks like the Duck (`duck`), and trace-leaving moves (`past`).
- **Potions and Cooldowns**: A system for limited-use or cooldown-based drops (e.g., `freezePotion`, `jumpPotion`).
- **Advanced Win Conditions**: Enhanced n-in-a-row (`connectN`) logic, including piece-type specific connection goals and support for multi-dimensional flattened boards.
- **Multimove and Alternation**: Support for irregular move counts and complex turn alternation patterns.
- **Baseline/Incomplete Tracking**: A dedicated `variants-incomplete.ini` to track WIP variants and their missing requirements.

## Contributing

If you are a developer looking to contribute, please refer to [AGENTS.md](AGENTS.md) for a technical overview of the codebase and guidelines for implementing new variant rules.

For standard Fairy-Stockfish functionality and supported variants, please refer to the [main Fairy-Stockfish repository](https://github.com/fairy-stockfish/Fairy-Stockfish).
