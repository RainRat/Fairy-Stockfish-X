#!/bin/bash

set -euo pipefail

error() {
  echo "swapping regression failed on line $1" >&2
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-./src/stockfish}
VARIANT_PATH=${2:-./src/variants.ini}

TMP_INI=$(mktemp)
trap 'rm -f "${TMP_INI}"' EXIT

cat > "${TMP_INI}" <<'INI'
[swap-basic:fairy]
maxFile = e
maxRank = 5
castling = false
checking = false
king = -
pieceToCharTable = -
customPiece1 = a:mW
customPiece2 = b:mW
adjacentSwapMoveTypes = a
adjacentSwapRequiresEmptyNeighbor = true
swapNoImmediateReturn = true
startFen = 5/5/5/5/5 w - - 0 1
INI

run_cmds() {
  local path=$1
  local variant=$2
  local cmds=$3
  cat <<EOF | "${ENGINE}" 2>/dev/null
uci
setoption name VariantPath value ${path}
setoption name UCI_Variant value ${variant}
${cmds}
quit
EOF
}

out=$(run_cmds "${TMP_INI}" swap-basic "position fen 5/5/2Ab1/5/5 w - - 0 1
go perft 1")
echo "${out}" | grep -q "^c3c2: 1$"
echo "${out}" | grep -q "^c3b3: 1$"
echo "${out}" | grep -q "^c3c4: 1$"
echo "${out}" | grep -q "^c3d3s: 1$"

out=$(run_cmds "${TMP_INI}" swap-basic "position fen 5/2a2/1aAb1/2a2/5 w - - 0 1
go perft 1")
! echo "${out}" | grep -q "^c3d3s: 1$"

out=$(run_cmds "${TMP_INI}" swap-basic "position fen 5/5/2Ab1/5/5 w - - 0 1 moves c3d3s
go perft 1")
! echo "${out}" | grep -q "^d3c3s: 1$"

out=$(run_cmds "${VARIANT_PATH}" lewthwaite-swap "position startpos
go perft 1")
echo "${out}" | grep -q "Nodes searched:"
! echo "${out}" | grep -q "s: 1$"

echo "swapping ok"
