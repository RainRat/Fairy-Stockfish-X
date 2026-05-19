#!/bin/bash
set -euo pipefail

# Tests that a walling variant correctly generates and validates a pure walling move
# without needing the dead walling block in pseudo_legal().

ENGINE="${1:-./src/stockfish}"
PWD_PATH=$(pwd)

cat << 'IN' > tests/temp_variants.ini
[walling-regression-test:atomic]
wallingRule = duck
IN

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

# Check if the board shows '*' at a3
if ! echo "$OUTPUT" | grep -E "\| \* \|   \|   \|   \| P \|   \|   \|   \|3" > /dev/null; then
    echo "Engine failed to apply the wall at a3 or pawn at e3."
    echo "$OUTPUT"
    false
fi

if ! echo "$OUTPUT" | grep -q "bestmove"; then
    echo "Engine failed to search or crashed."
    echo "$OUTPUT"
    false
fi

echo "Success!"
