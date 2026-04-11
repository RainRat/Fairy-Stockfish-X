# Fairy-Stockfish-X

Fairy-Stockfish-X is an experimental version of [Fairy-Stockfish](https://github.com/fairy-stockfish/Fairy-Stockfish). It is used to test new features and support unique chess variants.

## Quick Start

To build the engine, follow these steps:

1. Open your terminal and go to the `src/` directory.
2. Run the build command:
   ```bash
   make -j build ARCH=x86-64-modern
   ```
   *Note: If you have a different CPU, you can check other options by running `make help`.*

### Build Options

You can add flags to the `make` command to enable special features:

- **Large Boards:** For variants like Shogi (up to 12x10), add `largeboards=yes`.
- **Very Large Boards:** For boards up to 16x16, add `verylargeboards=yes`.
- **All Variants:** To include unusual variants like Game of the Amazons, add `all=yes`.

Example for Shogi:
```bash
make -j build ARCH=x86-64-modern largeboards=yes
```

## Basic Usage

After building, you can start the engine by running:
```bash
./stockfish
```

### Loading Variants

Fairy-Stockfish-X supports many variants through a configuration file. To load them:

1. Tell the engine where your variants file is:
   ```uci
   setoption name VariantPath value variants.ini
   ```
2. Choose a variant to play:
   ```uci
   setoption name UCI_Variant value antichess
   ```
3. Use the `d` command to see the current board.

## Python Bindings

You can also use Fairy-Stockfish-X in Python.

### Prerequisites
- Python 3.x
- `setuptools` (Install via `pip install setuptools`)

### Building the Extension
To build the Python extension, run this in the project root:
```bash
python3 setup.py build_ext --inplace
```
After building, you can import `pyffish` in your Python scripts.

## Purpose

This project has three main goals:
1. Test new features before they move to the main project.
2. Provide a place to experiment with new ideas.
3. Support chess variants that are too unusual for the standard engine.

For standard functionality, please visit the [main Fairy-Stockfish repository](https://github.com/fairy-stockfish/Fairy-Stockfish).
