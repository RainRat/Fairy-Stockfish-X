1. **Investigate the overflow limit (`MOVEGEN_OVERFLOW_CAPACITY`)**:
   - `MOVEGEN_OVERFLOW_CAPACITY` is `MAX_MOVES * 4`.
   - On the default `ALLVARS` build (which most users and python tests use), `MAX_MOVES = 16384`, so the limit is `65536`.
   - On `VERY_LARGE_BOARDS`, `MAX_MOVES = 65536`, limit is `262144`.
   - On basic build (without `ALLVARS`), `MAX_MOVES = 4096`, limit is `16384`.

2. **Reachability**:
   - Potion generated moves scale as $M \times S$, where $M$ is the number of base moves, and $S$ is the number of valid potion drop squares.
   - On an 8x8 board (basic build), $S$ can be 64. To reach the limit `16384`, we need $M > 256$. This is extremely easy to reach by adding just a few pieces to a hand (using drops) or by having custom pieces like Amazons.
   - I confirmed with a test script `test_overflow_more_pieces.py` that $M$ reaches ~1100 with just a few different types of pieces in the hand, which would generate >70k potion moves on an 8x8 board!
   - So the overflow is definitely reachable, and truncation occurs.

3. **Modify `generate_potion_moves`**:
   - Replace the silent `if (cur >= maxEnd) return maxEnd;` with an `assert(cur < maxEnd);` for developers.
   - And keep `if (cur >= maxEnd) return maxEnd;` as the fallback in release builds so it doesn't crash the engine, but explicitly logs or is documented as truncation. The task instructions say "replace silent truncation with an assert/fallback", which exactly means:
     ```cpp
     assert(cur < maxEnd);
     if (cur >= maxEnd)
         return maxEnd; // Fallback
     ```

4. **Add regression test**:
   - Create a variant in `test.py` via `ini_text` string, for example `[potion_overflow]`.
   - Load it in `test.py`.
   - The test will simply be to call `sf.legal_moves` on a stress-inducing FEN that we know generates $>65536$ moves. Because we add `assert(cur < maxEnd)` it would fail in debug builds, but in python test it will just verify that the number of moves returned is at least the capacity, or handle it gracefully.
   - Wait, if the python extension is compiled in debug mode (`NDEBUG` not defined), `assert` will crash the process, making the test fail, which proves the regression! But if it's release mode, the test should still pass or show truncation. Actually, in `test.py` I can just add a `test_potion_overflow` that checks that `sf.legal_moves(variant, fen)` does not crash (if in release) or just demonstrates the scenario. The prompt says "add a regression that would have overflowed".
   - Or maybe the test just creates a FEN, gets moves, and we can check if it reaches max capacity if we want to ensure the fallback works.

5. **Pre-commit Instructions**:
   - Call `pre_commit_instructions` before submitting to make sure proper testing, verifications, reviews and reflections are done.
