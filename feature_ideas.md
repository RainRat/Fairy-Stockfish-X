# Feature Ideas for Fairy-Stockfish-X

Fairy-Stockfish-X (FSX) is already an incredibly versatile engine for variant prototyping. However, there are several "standard" variant mechanics that are notably absent or could be more elegantly integrated into the core engine.

## 1. "Surprised it doesn't do this already" (Feature Ideas)

### 🧩 Fog of War (Dark Chess)
**Description:** Pieces only reveal squares they can "see" (i.e., squares they can move to or capture on). The opponent's pieces remain hidden unless within a friendly piece's vision.
**Why it's surprising:** It's a very popular mode on Lichess and Chess.com. While Stockfish is a perfect-information engine, adding a "visibility mask" and potentially an MCTS-based search for hidden information would be a major experimental feature for FSX.

### 🌀 Portals and Teleporters
**Description:** Specific squares (e.g., `e4` and `h4`) are linked. Moving a piece onto `e4` immediately teleports it to `h4` (and vice-versa).
**Why it's surprising:** Many exotic variants use portals. Currently, FS uses complex `multimoves` or `wallingRule` tricks to simulate complex boards, but first-class portal support in `variants.ini` (e.g., `portals = e4:h4 f3:c6`) would be much cleaner.

### 📉 Gravity Chess
**Description:** Pieces "fall" towards the bottom of the board (or towards a specific rank) if they are not supported by another piece or by specific terrain.
**Why it's surprising:** Vertical-oriented variants (like Connect-4, which FS already supports via `enclosingDrop=top`) could be extended to full "Gravity Chess" where pieces drop whenever a piece below them is captured.

### 🔥 Status Effects (Burning, Frozen, Shielded)
**Description:** Pieces can gain temporary or permanent statuses. 
- **Frozen:** Cannot move for N turns.
- **Burning:** Captured automatically after N turns unless "healed."
- **Shielded:** Immune to the next capture attempt.
**Why it's surprising:** FS has "Spell Chess" (potions) which are one-time drops. Adding persistent piece states would open up "RPG-chess" and "Battle-chess" possibilities.

### ⬆️ Piece Leveling and Evolution
**Description:** Pieces transform into more powerful versions after achieving certain milestones (e.g., N captures, surviving N moves, or reaching a specific rank).
**Why it's surprising:** While `moveMorphPieceType` exists, it is a simple mapping. A conditional evolution system (e.g., `evolveOnCapture = p:2:n` -> Pawn becomes Knight after 2 captures) would be a game-changer for progression-based variants.

### 🎲 Stochastic (Dice) Chess
**Description:** Random elements that affect move legality (e.g., a "dice roll" determines which piece type is allowed to move this turn).
**Why it's surprising:** Many physical board games use these mechanics. Stockfish's deterministic nature is a hurdle, but adding a "randomness seed" or "dice-aware" evaluation could be an interesting experimental direction.

### 🃏 Action Points (AP) System
**Description:** Instead of 1 move per turn, players have N action points. Moving a Pawn costs 1 AP, a Queen costs 3 AP, and a "Spell" costs 5 AP.
**Why it's surprising:** This is a fundamental mechanic in many strategy games. FS supports `multimoves`, but it's a fixed number per turn. An AP system would allow for much deeper resource management.

### 🏰 Multi-player Support (3+ Players)
**Description:** 3-player (hex or circular) or 4-player (cross-board) support.
**Why it's surprising:** Fairy-Stockfish is the "gold standard" for variants, but it is strictly 2-player. For a project with "X" (experimental) in the name, breaking the 2-player barrier would be the ultimate experimental feature.

### ⛰️ Terrain and Square Properties
**Description:** Different square types affect movement. 
- **Forest:** Riders (Rook/Bishop) can only move 2 squares through it.
- **Ice:** Pieces "slide" to the end of the ice patch.
- **Mountain:** Impassable for all but jumpers (Knights).
**Why it's surprising:** Currently, `mobilityRegion` can restrict movement, but it doesn't *modify* movement behavior dynamically.

---

## 2. Bugs and Technical Debt

### 🐞 `bench` Variant Inconsistency
- The `bench` command often fails to correctly initialize certain variants (like `checkers`) directly. Users have to use a clunky "set UCI option first, then bench" workflow. A unified initialization path for `bench` would be more reliable.

### 🐞 Checkers Multi-jump via "Pass"
- The current implementation of forced multi-jumps in Checkers uses a "pass" move (`f6f6`-style) to signal continuation. This is non-standard for UCI and confusing for GUI developers. A native "atomic multi-move" representation in the UCI protocol for FSX would be much better.

### 🐞 Bitboard Scaling Limits
- Board sizes beyond 16x16 or non-rectangular shapes (like L-shaped boards) are difficult to support due to the fixed-size bitboard architecture (`VERY_LARGE_BOARDS`). Moving towards a more dynamic or "sparse" bitboard representation would fix many edge-case variant bugs.

### 🐞 UCI Piece Overload
- For variants with 20+ custom pieces, the UCI piece mapping can become ambiguous or overflow standard character limits. A more robust way to communicate custom piece definitions to GUIs (perhaps via a JSON/YAML extension in UCI) would improve compatibility.
