#!/bin/bash
set -euo pipefail

# Tests that a walling variant correctly generates and validates a pure walling move
# without needing the dead walling block in pseudo_legal().

ENGINE="${1:-./src/stockfish}"
PWD_PATH=$(pwd)

# We use duck walling in an atomic base to avoid the "Cannot use kings" parser error.
# Internally duck walling generates moves using `make_gating`, so `is_gating(m)` is true.

cat << 'IN' > tests/temp_variants.ini
[walling-regression-test:atomic]
wallingRule = duck
IN

# We provide a position with a walling move possible, and verify that
# such a walling move can be parsed and executed.
# FEN: startpos
# Move: e2e3,e3a3 (move e2 to e3, place duck at a3)
# The `d` command output verifies the board state has '*' at a3.

OUTPUT=$("$ENGINE" << IN
setoption name VariantPath value $PWD_PATH/tests/temp_variants.ini
isready
setoption name UCI_Variant value walling-regression-test
isready
position fen rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 moves e2e3,e3a3
d
go depth 5
quit
IN
)

rm tests/temp_variants.ini

# `d` prints the board rows like ` | * |   |   |   | P |   |   |   |3 `
if ! echo "$OUTPUT" | grep -E "\| \* \|   \|   \|   \| P \|   \|   \|   \|3" > /dev/null; then
    echo "Engine failed to apply the wall at a3 or the pawn at e3."
    echo "$OUTPUT"
    false
fi

if ! echo "$OUTPUT" | grep -q "bestmove"; then
    echo "Engine failed to search or crashed."
    echo "$OUTPUT"
    false
fi

echo "Success!"
