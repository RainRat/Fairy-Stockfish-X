#!/bin/bash
# If pureWallMove uses is_gating(m), then walling moves ARE gating moves internally.
# But wait, why did the parsing fail when I set wallingRule AND seirawanGating?
# Because seirawanGating is an actual chess variant rule.
# But a walling move internally REUSES the gating mechanism of moves to store the wall placement square!
# Let's verify this in src/movegen.cpp where walling moves are generated.
