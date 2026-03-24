#!/bin/bash

set -euo pipefail

error() {
  echo "toroidal-chess test failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-./stockfish}
VARIANT_PATH=${2:-src/variants.ini}

run_cmds() {
  cat <<EOF | "${ENGINE}" 2>/dev/null
uci
setoption name VariantPath value ${VARIANT_PATH}
$1
quit
EOF
}

out=$(run_cmds "setoption name UCI_Variant value toroidal-chess
position startpos
d")
echo "${out}" | grep -q "Fen: r1b2b1r/pp4pp/n1pqkp1n/3pp3/3PP3/N1PQKP1N/PP4PP/R1B2B1R w - - 0 1"

out=$(run_cmds "setoption name UCI_Variant value toroidal-chess
position fen 1k6/8/8/8/8/8/4K3/R7 w - - 0 1
go perft 1")
echo "${out}" | grep -q "^a1h1: 1$"
echo "${out}" | grep -q "^a1a8: 1$"

out=$(run_cmds "setoption name UCI_Variant value toroidal-chess
position fen r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1
go perft 1")
! echo "${out}" | grep -q "^e1g1: 1$"
! echo "${out}" | grep -q "^e1c1: 1$"

echo "toroidal-chess test OK"
