#!/bin/bash

set -euo pipefail

error() {
  echo "new-variants smoke test failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-./stockfish}
VARIANT_PATH=${2:-variants.ini}

run_cmds() {
  cat <<EOF | "${ENGINE}"
uci
setoption name VariantPath value ${VARIANT_PATH}
$1
quit
EOF
}

echo "new variants smoke testing started"

# 1) Hasami: orthogonal sandwich should capture the middle piece.
out=$(run_cmds "setoption name UCI_Variant value hasami
position fen 9/9/9/9/9/9/9/R1rR5/9 w - - 0 1 moves a2b2
d")
echo "${out}" | grep -q "Fen: 9/9/9/9/9/9/9/1R1R5/9 b - - 1 1"

# 2) Achi: pre-connected line is immediate game end (no legal moves).
out=$(run_cmds "setoption name UCI_Variant value achi
position fen PPP/3/3[PPPPpppp] b - - 0 1
go perft 1")
echo "${out}" | grep -q "Nodes searched: 0"

# 3) Checkless: king capture is legal (checks are disabled by variant).
out=$(run_cmds "setoption name UCI_Variant value checkless
position fen 4k3/8/8/8/8/8/4Q3/4K3 w - - 0 1
go perft 1")
echo "${out}" | grep -q "^e2e8: 1$"

# 4) Partisans: pawn captures only forward-left from each side perspective.
out=$(run_cmds "setoption name UCI_Variant value partisans
position fen 8/8/8/8/8/2p1p3/3P4/8 w - - 0 1
go perft 1")
echo "${out}" | grep -q "^d2c3: 1$"
echo "${out}" | grep -q "^d2d4: 1$"
! echo "${out}" | grep -q "^d2e3:"

echo "new variants smoke testing OK"
