# Universal Ray and Hopper Parameterization Proposal

## 1. Objective
To replace ad-hoc hardcoded piece modifiers (like `p` for Cannon, `g` for Grasshopper, and `jumpCaptureTypes`) with a generalized, parametric state-machine. This system defines how rays interact with pieces on the board, natively supporting Checkers-style locusts, Fairy Chess hoppers, and game-specific piece transparencies without needing explicit C++ implementations for every new piece type.

## 2. Syntax & INI Configuration
To maintain consistency with existing Betza modifiers (which precede the movement atom, e.g., `gQ`, `pQ`), the hopper parameters will be enclosed in curly braces `{}` and placed **before** the piece movement letter.

**Format:** `{key: value; key: value}Atom`

### Parameter Dictionary
*   **`hurdles`**: `min, max` — How many pieces must the ray pass over? (Use `*` for infinity).
*   **`pre`**: `min, max` — Distance (in squares) from the starting square to the *first* hurdle.
*   **`post`**: `min, max` — Distance (in squares) from the *last* hurdle to the destination square.
*   **`capture`**: Defines how captures are resolved if jumping over an enemy.
    *   `dest` (Default) — Captures whatever is on the landing destination square.
    *   `locust_all` — Lands on an empty square; removes *all* pieces that comprised the hurdle.
    *   `locust_first` — Lands on an empty square; removes only the *first* piece in the hurdle.
    *   `locust_last` — Lands on an empty square; removes only the *last* piece in the hurdle.
*   **`equi`**: Relational symmetry rules.
    *   `hopper` — Enforces `pre_distance == post_distance`.
    *   `stopper` — The destination is exactly halfway to the hurdle.
*   **`hurdle_types`**: Comma-separated list of what counts as a valid hurdle.
    *   Options: `enemy, friendly, wall, dead` (Defaults to `enemy, friendly`).
*   **`transparent_types`**: Comma-separated list of what is completely ignored by the ray.
    *   Options: `enemy, friendly, wall, dead` (Defaults to none).

---

## 3. Fulfilling the Priorities (Examples)

### Priority 1: Backward Compatibility
The existing lowercase `p` (cannon) and `g` (grasshopper) modifiers become syntactic sugar for their parametric equivalents during Betza string parsing:
*   `gQ` (Grasshopper) maps to: `{hurdles: 1,1; pre: 1,*; post: 1,1}Q`
*   `pQ` (Cannon, hopping capture) maps to: `{hurdles: 1,1; pre: 1,*; post: 1,*}cQ`

### Priority 2: Popular Known Games
*   **Checkers King (Locust-style capture):**
    Moves 1 square diagonally, but captures by jumping exactly 1 adjacent enemy, landing 1 square behind it, and removing the hurdle.
    `mF{hurdles: 1,1; pre: 1,1; post: 1,1; capture: locust_all; hurdle_types: enemy}cF`
*   **Lines of Action (LoA) Piece:**
    Passes over friendly pieces as if they aren't there.
    `{transparent_types: friendly}Q`

### Priority 3: Fairy Literature Favorites
*   **107 Equihopper:**
    Hops over a piece, landing at the mirror-opposite distance.
    `{hurdles: 1,1; equi: hopper}Q`
*   **102 ContraGrasshopper (CG):**
    Must be adjacent to the hurdle (pre=1), but can land any distance beyond it.
    `{hurdles: 1,1; pre: 1,1; post: 1,*}Q`

### Priority 4: Niche Glossary Hoppers
*   **99 Lion (LI):**
    Any distance to the hurdle, any distance after.
    `{hurdles: 1,1; pre: 1,*; post: 1,*}Q`
*   **124 Kangaroo (KA):**
    Hurdles exactly *two* units, landing on the first square beyond the second unit.
    `{hurdles: 2,2; pre: 1,*; post: 1,1}Q`
*   **129 Bob (BO):**
    Hurdles exactly *four* units, landing on the first square beyond.
    `{hurdles: 4,4; pre: 1,*; post: 1,1}Q`
*   **109 Nonstop Equihopper:**
    Makes an Equihopper move, but cannot be blocked by any other intermediate units (they are fully transparent).
    `{hurdles: 1,1; equi: hopper; transparent_types: enemy, friendly, wall, dead}Q`
*   **130 Equistopper:**
    Moves exactly halfway to the hurdle.
    `{hurdles: 1,1; equi: stopper}Q`

---

## 4. Implementation Concept: The Universal Ray Engine

In the move generator, instead of branching logic (`if (is_grasshopper) ... else if (is_locust) ...`), rays are evaluated using a continuous tracking loop. This efficiently solves multi-piece jumps (like the Kangaroo) without extra complexity.

```cpp
int distance = 0;
int hurdles_hit = 0;
int distance_to_first_hurdle = 0;
int distance_from_last_hurdle = 0;
std::vector<Square> hurdle_squares;

for (Square sq = start; is_valid(sq); sq += direction) {
    distance++;
    Piece p = board.piece_at(sq);

    if (p != EMPTY || is_wall(sq) || is_dead(sq)) {
        if (profile.transparent_types & type_of(p)) {
            // Ignored completely (e.g., LoA friendly pieces, Nonstop intermediate pieces)
            distance_from_last_hurdle++; 
            continue; 
        }
        
        if (profile.hurdle_types & type_of(p)) {
            hurdles_hit++;
            if (hurdles_hit == 1) distance_to_first_hurdle = distance;
            distance_from_last_hurdle = 0; // Reset counter after clearing a hurdle
            hurdle_squares.push_back(sq);
            
            if (hurdles_hit > profile.hurdles_max) break; // Exceeded allowed hurdles, ray blocked
            continue;
        }
        
        break; // Hit a piece/wall that is neither transparent nor a hurdle -> Blocked.
    } else {
        // Empty square
        distance_from_last_hurdle++;
    }

    // --- Validation Phase: Can the piece legally stop here? ---
    if (hurdles_hit >= profile.hurdles_min && hurdles_hit <= profile.hurdles_max) {
        if (distance_to_first_hurdle >= profile.pre_min && distance_to_first_hurdle <= profile.pre_max) {
            if (distance_from_last_hurdle >= profile.post_min && distance_from_last_hurdle <= profile.post_max) {
                
                // Relational rules
                if (profile.equi == EQUIHOPPER && distance_from_last_hurdle != distance_to_first_hurdle) continue;
                if (profile.equi == EQUISTOPPER && distance_to_first_hurdle != (distance / 2)) continue;
                
                // Add valid move
                add_move(sq, profile.capture_mode, hurdle_squares);
            }
        }
    }
}
```
