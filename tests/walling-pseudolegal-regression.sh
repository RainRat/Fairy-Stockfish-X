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

# We provide a position with gating possible, and let the engine search.
# In a search, it reads from the TT which invokes pseudo_legal(m).
# If it crashes, the script fails.

OUTPUT=$("$ENGINE" << 'IN'
setoption name VariantPath value tests/temp_variants.ini
setoption name UCI_Variant value walling-gating-test
isready
position fen rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR[EHAeha] w KQkq - 0 1
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
