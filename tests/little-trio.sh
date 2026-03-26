#!/bin/bash

set -euo pipefail

error() {
  echo "little-trio regression failed on line $1" >&2
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-./stockfish}
VARIANTS=${2:-variants.ini}

run_cmds() {
  cat <<EOF | "${ENGINE}" 2>/dev/null
uci
setoption name VariantPath value ${VARIANTS}
setoption name UCI_Variant value little-trio
$1
quit
EOF
}

# Start position should show the intended Xiangqi cannon capture over the pawn screen.
out=$(run_cmds "position startpos
go perft 1")
echo "${out}" | grep -q "f1f6: 1"

# Capturing a Shogi piece should transfer it to hand.
out=$(run_cmds "position fen s3k2/7/7/7/7/7/R3K2[] w - - 0 1 moves a1a7
d")
echo "${out}" | grep -q "Fen: R3k2/7/7/7/7/7/4K2\\[S\\] b - - 0 1"

# Capturing a Xiangqi piece should not transfer it to hand.
out=$(run_cmds "position fen c3k2/7/7/7/7/7/R3K2[] w - - 0 1 moves a1a7
d")
echo "${out}" | grep -q "Fen: R3k2/7/7/7/7/7/4K2\\[\\] b - - 0 1"

# Lances may not be dropped on the farthest rank.
out=$(run_cmds "position fen 4k2/7/7/7/7/7/4K2[L] w - - 0 1
go perft 1")
echo "${out}" | grep -q "L@a6: 1"
! echo "${out}" | grep -q "L@a7: 1"
