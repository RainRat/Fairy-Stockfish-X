#!/bin/bash

set -euo pipefail

ENGINE=${1:-src/stockfish}
VARIANT_PATH=${2:-src/variants.ini}

run_cmds() {
  cat <<EOF | "${ENGINE}"
uci
setoption name VariantPath value ${VARIANT_PATH}
$1
quit
EOF
}

echo "constabulary-chess test started"

out=$(run_cmds "setoption name UCI_Variant value constabulary-chess
position startpos
d")
echo "${out}" | grep -q "Fen: wxeiiexw/rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR/WXEIIEXW w KQkq - 0 1"

out=$(run_cmds "setoption name UCI_Variant value constabulary-chess
position fen 8/8/8/8/8/8/8/8/R3K2R/8 w KQ - 0 1 moves e2g2
d")
echo "${out}" | grep -q "Fen: 8/8/8/8/8/8/8/8/R4RK1/8 b - - 1 1"

out=$(run_cmds "setoption name UCI_Variant value constabulary-chess
position fen 7k/P7/8/8/8/8/8/8/8/7K w - - 0 1
go perft 1")
echo "${out}" | grep -q "^a9a10q: 1$"
echo "${out}" | grep -q "^a9a10w: 1$"
echo "${out}" | grep -q "^a9a10x: 1$"
echo "${out}" | grep -q "^a9a10e: 1$"
echo "${out}" | grep -q "^a9a10i: 1$"

echo "constabulary-chess test OK"
