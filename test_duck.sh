#!/bin/bash
set -euo pipefail

ENGINE="${1:-./src/stockfish}"
OUTPUT=$("$ENGINE" << IN
setoption name UCI_Variant value atomicduck
isready
position startpos moves e2e3@@a3
go depth 2
quit
IN
)
echo "$OUTPUT" | grep "pv"
