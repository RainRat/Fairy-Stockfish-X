#!/bin/bash

set -euo pipefail

error() {
  echo "cylinder test failed on line $1"
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

out=$(run_cmds "setoption name UCI_Variant value cylinder
position fen 4k3/8/8/8/8/8/8/R3K3 w - - 0 1
go perft 1")
echo "${out}" | grep -q "^a1h1: 1$"

out=$(run_cmds "setoption name UCI_Variant value cylinder
position fen r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1
go perft 1")
! echo "${out}" | grep -q "^e1g1: 1$"
! echo "${out}" | grep -q "^e1c1: 1$"

out=$(run_cmds "setoption name UCI_Variant value cylinder-castling
position fen r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1
go perft 1")
echo "${out}" | grep -q "^e1g1: 1$"
echo "${out}" | grep -q "^e1c1: 1$"

echo "cylinder test OK"
