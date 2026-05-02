# Fairy-Stockfish-X Code Review & Issue Report

This document synthesizes verified issues and areas for improvement found in the Fairy-Stockfish-X codebase, specifically tailored with technical details for a coding agent to execute fixes. Debunked or inaccurate reports have been omitted.

## 1. Architectural & Performance Issues

### 1.1 Inefficient LRU Cache Eviction (`src/bitboard.cpp`)
- **Issue**: `Bitboards::init_magics` manages caching for `MagicGeometry` using a `std::vector<uint16_t> MagicCacheLru`. Eviction logic uses `std::find` followed by `erase` on this vector (around lines 928-932 and 996-999).
- **Impact**: This leads to an O(n) operation for every cache hit/miss, which is inefficient and slows down initialization for large boards.
- **Actionable Fix**: Replace `std::vector<uint16_t> MagicCacheLru` with a `std::list` or a more efficient LRU cache data structure that allows O(1) removals and insertions.

### 1.2 Memory Leak Risk in FFI (`src/ffishdll.cpp`)
- **Issue**: The `to_cstr` function allocates memory using `new char[s.size() + 1]` (around line 78) and returns a raw pointer to be passed through the C API.
- **Impact**: Callers of the DLL API are forced to manually call `fsf_free`, which is highly error-prone and leads to memory leaks if omitted.
- **Actionable Fix**: Review lifetime management. Consider managing the string memory internally (e.g., using `thread_local` static strings for temporary returns, or explicitly documenting memory ownership rules).

### 1.3 Unsafe `const_cast` in `PieceMap::add` (`src/piece.cpp`)
- **Issue**: `PieceMap::add` receives a `const PieceInfo* p` but casts it to non-const to modify its properties `mobilityScaling` and `diagonalLimitedSlider` (around lines 640-641: `const_cast<PieceInfo*>(p)->mobilityScaling = ...;`).
- **Impact**: Modifying a const object is undefined behavior and breaks the function contract.
- **Actionable Fix**: Ensure the `PieceInfo` is modified *before* it is passed as `const` to the function, or change the function signature if taking ownership/modifying is intended.

### 1.4 Global State & Hostile Error Handling (`src/misc.cpp`)
- **Issue**: The `Logger::start` function defines a local static `static Logger l;`. If the file fails to open, it calls `exit(EXIT_FAILURE)` (line 135).
- **Impact**: Calling `exit()` inside a library (`ffishdll`) forcefully kills the host process.
- **Actionable Fix**: Replace `exit(EXIT_FAILURE)` with a robust error-handling mechanism (e.g., returning an error code or throwing a catchable exception) so the host process can handle the error gracefully.

## 2. Variant & Special Piece Movement Bugs

### 2.1 Hardcoded Distance in `lame_leaper_path` (`src/bitboard.cpp`)
- **Issue**: In `lame_leaper_path` (around line 284), there is a hardcoded check: `if (!is_ok(to) || distance(s, to) >= 4) return b;`.
- **Impact**: Custom lame leapers (e.g., a Giraffe 4,1) with a coordinate distance difference of 4 or more will have an empty path returned, effectively ignoring their "lame" (blockable) property and incorrectly allowing them to jump over pieces.
- **Actionable Fix**: Remove the hardcoded `distance(s, to) >= 4` limit and allow the function to calculate the blockable path regardless of leap distance, or dynamically configure the limit.

### 2.2 Hardcoded Rose Movement Logic (`src/bitboard.h`)
- **Issue**: The `rose_between_intersection_bb` function (around line 366) loops over a fixed `RoseSteps[index]` array representing knight-step vectors and caps the iteration at exactly 7 legs (`for (int leg = 0; leg < 7; ++leg)`).
- **Impact**: Custom Rose riders with non-knight steps (e.g., a Camel-Rose) or playing on boards large enough to support more than 7 legs before looping will be evaluated incorrectly.
- **Actionable Fix**: Remove the hardcoded 7-leg limit and use dynamic cycle length checks based on the specific leap vectors of the Rose piece type. Extract the base steps dynamically rather than using the hardcoded `RoseSteps`.

### 2.3 Janggi Elephant Betza Overwrite (`src/piece.cpp`)
- **Issue**: In `janggi_elephant_piece()` (around line 545), the betza string is explicitly overridden with `p->betza = "mafsmafW";` after being correctly parsed from `"nZ"`.
- **Impact**: The stored `betza` field no longer matches the piece's actual movement logic. This breaks any external usage of the field for move validation or UI representation.
- **Actionable Fix**: Store the XBoard/Winboard compatibility string in a separate field (e.g., `xboard_betza`) or only format it dynamically when explicitly required for protocol output.

### 2.4 Duplicate Move Generation in `commit_atom` (`src/piece.cpp`)
- **Issue**: The Betza parser's `commit_atom` function can generate duplicate moves in the internal representation (IR) when applying directional modifiers (e.g., `fW` adding `(1,0)` step twice).
- **Impact**: Causes inefficiency during move generation due to duplicate step evaluation.
- **Actionable Fix**: Implement deduplication logic or check for zero-deltas/symmetry overlaps before committing directional atoms in `commit_atom`.

## 3. End-Game Adjudication Logic Issues

### 3.1 Unbound Pieces Heuristic False Positives (`src/apiutil.h`)
- **Issue**: The `has_insufficient_material` function checks if an `unbound` piece has helper pieces. Some complex custom pieces in obscure variants can force mate alone against a lone King but might not be correctly recognized by the `unbound` heuristic as major pieces.
- **Impact**: The engine may prematurely adjudicate a position as a draw due to "insufficient material" when a forced mate is actually possible.
- **Actionable Fix**: Add a property to `PieceInfo` (or within the variant configuration) to explicitly flag custom pieces capable of lone-mating, and prioritize this flag in `has_insufficient_material`.

### 3.2 `connect_group` Search Performance (`src/position.cpp`)
- **Issue**: The `is_immediate_game_end` function evaluates `connect_group` goals using a BFS queue. Executing a full BFS on every single ply during deep search is highly expensive.
- **Impact**: This causes a significant NPS (nodes per second) drop in variants relying heavily on connection goals.
- **Actionable Fix**: Profile the BFS algorithm and consider caching connected components in the `Position` state, incrementally updating the graph during `do_move` and `undo_move`.

### 3.3 Syzygy Tablebase Variant Incompatibilities
- **Issue**: `tbprobe.cpp` does not heavily gate Tablebase probes behind variant compatibility checks.
- **Impact**: If a user configures `SyzygyPath` in a variant with incompatible rules (like `extinction`, custom pieces, or `points_goal`), the tablebase will erroneously return standard chess WDL results.
- **Actionable Fix**: In `Search::init()` or `tbprobe.cpp`, ensure Tablebases are explicitly disabled if `rootPos.variant()` specifies rules incompatible with standard Syzygy evaluation.
