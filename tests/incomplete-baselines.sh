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
position fen 5/5/1D1dD/5/5 w - - 0 1 moves b3c3
d")
echo "${out}" | grep -Eq "Fen: 5/5/2D1D/5/5(\\[\\])? b - - 1 1"

# Ko-app-paw-na baseline: hunter can hop-capture over one adjacent rabbit.
out=$(run_cmds "setoption name UCI_Variant value ko-app-paw-na
position fen 5/2R2/2h2/5/5 b - - 0 1 moves c3c5
d")
echo "${out}" | grep -q "Fen: 2h2/5/5/5/5 w - - 0 2"
