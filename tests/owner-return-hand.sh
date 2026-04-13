#!/bin/bash

set -euo pipefail

error() {
  echo "owner-return-hand test failed on line $1" >&2
  exit 1
}
trap 'error ${LINENO}' ERR

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ENGINE="${1:-${SCRIPT_DIR}/../src/stockfish-large}"

tmp_ini=$(mktemp)
cat > "${tmp_ini}" <<'INI'
[owner-hand-capture:fairy]
maxRank = 3
maxFile = c
pieceToCharTable = -
king = -
customPiece1 = a:W
startFen = 3/1a1/1A1[A] w - - 0 1
checking = false
captureType = hand
captureToHandSide = owner

[owner-hand-eject:fairy]
maxRank = 5
maxFile = e
pieceToCharTable = -
king = -
customPiece1 = a:-
startFen = AAAAa/5/5/5/5[A] w - - 0 1
pieceDrops = true
mustDrop = true
checking = false
nMoveRule = 0
captureType = hand
captureToHandSide = owner
edgeInsertOnly = true
dropRegionWhite = a1 a2 a3 a4 a5 b5 c5 d5 e5
edgeInsertTypes = a
edgeInsertRegionWhite = a1 a2 a3 a4 a5 b5 c5 d5 e5
edgeInsertFromWhite = top left
pushingStrength = a:5
pushFirstColor = either
pushingRemoves = shove
INI

run_cmds() {
  cat <<EOF | "${ENGINE}" 2>/dev/null
uci
setoption name VariantPath value ${tmp_ini}
$1
quit
EOF
}

out=$(run_cmds "setoption name UCI_Variant value owner-hand-capture
position startpos moves b1b2
d")
echo "${out}" | grep -q "Sfen: 3/1A1/3 w Aa 2"

out=$(run_cmds "setoption name UCI_Variant value owner-hand-eject
position startpos moves A@a1,b1
d")
echo "${out}" | grep -q "Fen: AAAAa/5/5/5/A4\\[\\] b - - 0 1"

rm -f "${tmp_ini}"
echo "owner-return-hand tests passed"
