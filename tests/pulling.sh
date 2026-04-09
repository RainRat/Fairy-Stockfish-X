#!/bin/bash

set -euo pipefail

error() {
  echo "pulling regression failed on line $1" >&2
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-./src/stockfish}

TMP_INI=$(mktemp)
trap 'rm -f "${TMP_INI}"' EXIT

cat > "${TMP_INI}" <<'INI'
[pull-basic:fairy]
maxFile = e
maxRank = 5
castling = false
checking = false
king = -
pieceToCharTable = -
customPiece1 = a:mW
customPiece2 = b:mW
customPiece3 = c:mW
pullingStrength = a:3 b:1 c:3
startFen = 5/5/5/5/5 w - - 0 1
INI

run_cmds() {
  local variant=$1
  local cmds=$2
  cat <<EOF | "${ENGINE}" 2>/dev/null
uci
setoption name VariantPath value ${TMP_INI}
setoption name UCI_Variant value ${variant}
${cmds}
quit
EOF
}

out=$(run_cmds pull-basic "position fen 5/5/2b2/2A2/5 w - - 0 1
go perft 1")
echo "${out}" | grep -q "^c2d2: 1$"
echo "${out}" | grep -q "^c2d2,c3: 1$"

out=$(run_cmds pull-basic "position fen 5/5/2c2/2A2/5 w - - 0 1
go perft 1")
! echo "${out}" | grep -q "^c2d2,c3: 1$"

out=$(run_cmds pull-basic "position fen 5/5/2b2/2A2/5 w - - 0 1 moves c2d2,c3
d")
echo "${out}" | grep -q "Fen: 5/5/5/2bA1/5 b - - 1 1"

echo "pulling ok"
