# Proposal: Convert Blast Booleans into a unified `blastPattern`

## 1. Motivation
The current implementation of blast (explosion) shapes uses three hardcoded booleans: `blastDiagonals`, `blastOrthogonals`, and `blastCenter`. This limits the shape of explosions to either 3x3 squares (or subsets thereof) and restricts variant creators from experimenting with more creative explosion patterns such as Knight-jumps (`N`), pure diagonal (`F`), or asymmetric blasts.

## 2. Proposed Changes
- Deprecate `blastDiagonals`, `blastOrthogonals`, and `blastCenter`.
- Introduce a new option `blastPattern` of type `std::string` in `Variant`.
- `blastPattern` accepts a Betza notation string. During parsing or variant initialization, we parse this Betza string into a `PieceInfo` and evaluate its attack mask around a central square (e.g. `SQ_D4`). This mask acts as the "relative bitboard mask" for explosions.
- To support explosions on the center square itself (currently `blastCenter`), we can extend the syntax or allow an explicit center inclusion character (like `*` or `c`), OR default to including the center unless a specific flag is set. However, a simpler relative bitboard syntax could be supported using coordinate offsets (e.g. `(0,0)`, `(1,2)`), but a Betza string is much more compact and familiar to Fairy-Stockfish users.
- We will store the pre-calculated blast mask for each square in a `Bitboard blastMask[SQUARE_NB]` array inside `Variant`, which is generated once during `check_consistency()` or `init_magics()`.

## 3. Implementation Details
1. Add `std::string blastPattern = "K*";` to `Variant`. (Using `*` as a custom suffix to indicate the center square is included, which is typical for atomic chess).
2. Remove `blastDiagonals`, `blastOrthogonals`, `blastCenter` from `Variant` and `position.h`.
3. In `position.h`, `blast_pattern(Square to)` will return `var->blastMask[to]` excluding the center, and `blast_squares(Square to)` will use `var->blastMask[to]` directly.
4. During `check_consistency` in `parser.cpp`, parse the `blastPattern` string. If it contains `*` (or another center-designator), set a flag for center inclusion. Pass the rest to `from_betza`. Then for each square `s`, use `attacks_bb()` or a custom routine to calculate the bitboard of squares hit by that Betza piece from `s`, and store it in `v->blastMask[s]`.

## 4. Backwards Compatibility
In `VariantParser::parse_attribute`, we can still parse the legacy `blastDiagonals`, `blastOrthogonals`, and `blastCenter` attributes, and internally translate them into an equivalent `blastPattern`.
- `blastDiagonals=true, blastOrthogonals=true, blastCenter=true` -> `blastPattern = "K*"`
- `blastDiagonals=false, blastOrthogonals=true, blastCenter=true` -> `blastPattern = "W*"`
- etc.

This provides an immediate upgrade path without breaking existing variants.ini files.