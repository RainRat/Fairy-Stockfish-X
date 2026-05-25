#!/bin/bash

set -euo pipefail

error() {
  echo "seega regression failed on line $1" >&2
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-./stockfish}
VARIANT_PATH=${2:-src/variants.ini}

run_cmds() {
  cat <<EOF | "${ENGINE}"
uci
setoption name VariantPath value ${VARIANT_PATH}
setoption name UCI_Variant value seega
$1
quit
EOF
}

echo "seega regression started"

out=$(run_cmds "position startpos moves D@a1
go perft 1")
echo "${out}" | grep -q "^0000: 1$"
! echo "${out}" | grep -q "^D@"

out=$(run_cmds "position startpos moves D@a1 0000
go perft 1")
echo "${out}" | grep -q "^D@"
! echo "${out}" | grep -q "^0000: 1$"

out=$(run_cmds "position startpos moves D@a1 0000 D@b1
d")
echo "${out}" | grep -Eq "^Fen: .* b "

out=$(run_cmds "position fen d4/5/1D1dD/5/d4 w - - 0 1 moves b3c3
d")
echo "${out}" | grep -Eq "Fen: d4/5/2D1D/5/d4(\\[\\])? b - - 1 1"

out=$(run_cmds "position fen 5/2D2/1DdD1/D1D2/dD3 b - - 0 1
go perft 1")
echo "${out}" | grep -q "^0000: 1$"

out=$(run_cmds "position fen 5/5/5/5/1D3[] b - - 0 1
go movetime 20")
echo "${out}" | grep -q "^info depth 0 score mate 0$"
echo "${out}" | grep -q "^bestmove (none)$"

echo "seega regression passed"
