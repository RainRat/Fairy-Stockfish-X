#!/bin/bash

set -euo pipefail

error() {
  echo "incomplete baselines test failed on line $1" >&2
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-./stockfish}
VARIANT_PATH=${2:-src/variants-incomplete.ini}

if [[ ! -f "${VARIANT_PATH}" && -f "variants-incomplete.ini" ]]; then
  VARIANT_PATH="variants-incomplete.ini"
fi

run_cmds() {
  cat <<EOF | "${ENGINE}" 2>/dev/null
uci
setoption name VariantPath value ${VARIANT_PATH}
$1
quit
EOF
}

# Seega baseline: opening setup excludes the center square.
out=$(run_cmds "setoption name UCI_Variant value seega
position startpos
go perft 1")
! echo "${out}" | grep -q "^D@c3:"

# Seega baseline: custodial capture removes the sandwiched piece.
out=$(run_cmds "setoption name UCI_Variant value seega
position fen d4/5/1D1dD/5/d4 w - - 0 1 moves b3c3
d")
echo "${out}" | grep -Eq "Fen: d4/5/2D1D/5/d4(\\[\\])? b - - 1 1"

# Seega baseline: a blocked side passes rather than losing immediately.
out=$(run_cmds "setoption name UCI_Variant value seega
position fen 5/2D2/1DdD1/D1D2/dD3 b - - 0 1
go perft 1")
echo "${out}" | grep -q "^0000: 1$"

# Seega baseline: extinction is surfaced as an immediate terminal loss.
out=$(run_cmds "setoption name UCI_Variant value seega
position fen 5/5/5/5/1D3[] b - - 0 1
go movetime 20")
echo "${out}" | grep -q "^info depth 0 score mate 0$"
echo "${out}" | grep -q "^bestmove (none)$"
