#!/bin/bash
set -euo pipefail

# Tests that a walling variant correctly evaluates pseudo_legal()
# paths without crashing or returning errors after removing the dead code block.

ENGINE="${1:-./src/stockfish}"
PWD_PATH=$(pwd)

# We use duck walling.
# Internally duck walling generates moves using `make_gating`, so
# `is_gating(m)` returns true.
# By running a search to depth 5, the engine will explore many walling moves
# and store/retrieve them from the Transposition Table (TT).
# TT retrieval invokes `pseudo_legal(m)`.
# If `pseudo_legal(m)` fails or crashes for these walling moves, the search fails.

cat << 'IN' > tests/temp_variants.ini
[walling-regression-test:chess]
wallingRule = duck
IN

OUTPUT=$("$ENGINE" << IN
setoption name VariantPath value $PWD_PATH/tests/temp_variants.ini
setoption name UCI_Variant value walling-regression-test
isready
position fen rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1
go depth 5
quit
IN
)

rm tests/temp_variants.ini

if ! echo "$OUTPUT" | grep -q "bestmove"; then
    echo "Engine failed to search or crashed."
    echo "$OUTPUT"
    false
fi

echo "Success!"
