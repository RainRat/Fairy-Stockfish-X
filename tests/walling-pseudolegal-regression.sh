#!/bin/bash
set -euo pipefail

# Tests that a walling variant with gating moves properly evaluates pseudo_legal()
# paths without crashing or returning errors after removing the dead code block.

ENGINE="${1:-./src/stockfish}"
PWD_PATH=$(pwd)

cat << 'IN' > tests/temp_variants.ini
[walling-gating-test:chess]
wallingRule = duck
seirawanGating = true
IN

OUTPUT=$("$ENGINE" << IN
setoption name VariantPath value $PWD_PATH/tests/temp_variants.ini
setoption name UCI_Variant value walling-gating-test
isready
position fen rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR[EHAeha] w KQkq - 0 1 moves b1c3~H@a3
go depth 5
quit
IN
)

rm tests/temp_variants.ini

if ! echo "$OUTPUT" | grep -q "bestmove"; then
    echo "Engine failed to search or crashed."
    false
fi

echo "Success!"
