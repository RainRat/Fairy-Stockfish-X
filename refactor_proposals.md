# Fairy-Stockfish-X Variant Option Refactoring Proposals

Based on an analysis of the configuration parsers, struct definitions, and option usages, the following proposals aim to consolidate single-purpose options, reduce boilerplate parsing, and eliminate redundant option families in `variants.ini`.

## 1. Unify Region Override Options (Step / Drop / Promotion Regions)
**Current state:** There are massive families of 5 separate options controlling regions (e.g., `doubleStepRegionWhite`, `doubleStepRegionBlack`, `pieceSpecificDoubleStepRegion`, `whitePieceDoubleStepRegion`, `blackPieceDoubleStepRegion`). This pattern is repeated for `tripleStepRegion`, `dropRegion`, and `promotionRegion`.
**Proposal:** The existing `PieceTypeBitboardGroup` type parses piece-specific bitboards (e.g., `P(e4,*1)`). By enhancing its parser to support a fallback wildcard piece `*(...)`, we can replace all 5 options with a single `[Action]Region` setting. For example: `doubleStepRegion = P(e4,*1); *(**)` allows specific piece overrides while defining a global fallback, entirely deprecating the `pieceSpecific...` and `[color]Piece...` options and significantly speeding up internal `position.h` logic.

## 2. Refactor Color-Specific Options to use `ColorSetting<T>`
**Current state:** A huge source of bloat is options duplicated for colors (e.g., `mustDropType`, `mustDropTypeWhite`, `mustDropTypeBlack`). Currently, the codebase handles this with manually duplicated arrays like `v->dropNoDoubledByColor[WHITE] = v->dropNoDoubled;` scattered all over `parser.cpp`.
**Proposal:** `Fairy-Stockfish-X` introduced an excellent generic wrapper, `ColorSetting<T>`, that cleanly isolates global values from explicit color overrides using a safe `.has_override(c)` API. However, it was never fully adopted. I propose aggressively replacing the ad-hoc `T[COLOR_NB]` arrays and boilerplate blocks for options like `mustDrop`, `mustDropType`, `dropChecks`, `mustCapture`, `pass`, and `dropNoDoubled` with `ColorSetting<T>`.

## 3. Merge `promotionPieceTypesByFile` into `promotionPieceTypes`
**Current state:** Pawn promotions are defined either globally (`promotionPieceTypes = nbrq`) or per-file via a completely different option (`promotionPieceTypesByFile = a:r b:n`), each of which *also* has White/Black variants.
**Proposal:** Enhance the standard `promotionPieceTypes` parser into a smarter format that accepts either a flat `PieceSet` (legacy) or a `File->PieceSet map`. By adding support for a file fallback wildcard (e.g., `promotionPieceTypes = a:r b:n *:q`), the engine can entirely deprecate the `...ByFile` option family.

## 4. Enhance `dropNoDoubled` to handle `PieceSet`
**Current state:** The `dropNoDoubled` rule (which prevents dropping a piece on a file that already has one, like the Shogi pawn) is hardcoded to accept a single `PieceType`. If a variant designer wants to restrict both pawns *and* lances, they can't.
**Proposal:** Change `dropNoDoubled` from a single `PieceType` to a `PieceSet`. This allows variant designers to restrict multiple piece types at once (e.g., `dropNoDoubled = p l`). Furthermore, integrating it with `ColorSetting<PieceSet>` (from Proposal 2) safely deprecates `dropNoDoubledWhite` and `dropNoDoubledBlack`.

## 5. Convert Blast Booleans into a unified `blastPattern` 
**Current state:** Atomic and explosion variants determine the shape of the blast using three hardcoded, single-purpose booleans: `blastDiagonals`, `blastOrthogonals`, and `blastCenter`.
**Proposal:** Deprecate these three booleans in favor of a new `blastPattern` option. This could accept a Betza string (e.g., `K` for the standard 3x3 square, `N` for knight-jump explosions, or `W` for orthogonal crosses) or a relative `Bitboard` mask. This provides infinite shape flexibility for variant creators while shrinking the option count.

## 6. Consolidate `edgeInsert` direction logic
**Current state:** Edge insertion drops require users to specify the board edges as direction tokens (`edgeInsertFrom = top left`) and the actual entry squares (`edgeInsertRegion = *8 a*`). This redundancy requires 6 underlying arrays in `Variant` just to track direction aliases.
**Proposal:** The engine should infer the valid push directions mathematically based on the closest physical edge to the `edgeInsertRegion` bitboards. This removes the need for `edgeInsertFrom` entirely, trusting the bitboard geometry instead.
