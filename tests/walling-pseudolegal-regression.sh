#!/bin/bash
set -euo pipefail

# Tests that a walling variant with gating moves properly evaluates pseudo_legal()
# paths without crashing or returning errors after removing the dead code block.

ENGINE="${1:-./src/stockfish}"

cat << 'IN' > tests/temp_variants.ini
[walling-gating-test:chess]
wallingRule = duck
seirawanGating = true
IN

# We provide a position with gating possible, and verify that
# such a gating+walling move can be generated, and then applied/undone
# correctly without crashes or invalid evaluations.
# FEN: startpos, but we make a move that gates H (Hawk) and places a wall at a3.
# The `go depth 5` also triggers TT insertion/lookup using `pseudo_legal()`.

OUTPUT=$("$ENGINE" << 'IN'
setoption name VariantPath value tests/temp_variants.ini
setoption name UCI_Variant value walling-gating-test
isready
position fen rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR[EHAeha] w KQkq - 0 1 moves b1c3~H@a3
go depth 5
quit
IN
)

rm tests/temp_variants.ini

if ! echo "$OUTPUT" | grep -q "bestmove"; then
    echo "Engine failed to generate or evaluate the gating+walling move."
    false
fi

echo "Success!"
