#!/bin/bash
set -euo pipefail

ENGINE="${1:-./src/stockfish}"
PWD_PATH=$(pwd)

OUTPUT=$("$ENGINE" << IN
setoption name VariantPath value ${PWD_PATH}/temp_variants.ini
isready
setoption name UCI_Variant value walling-duck
isready
position fen rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1
go depth 2
quit
IN
)
echo "$OUTPUT" | grep "info depth 1"
