#!/usr/bin/env bash
set -euo pipefail

error() {
  echo "connect-region3 regression failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE="${1:-./src/stockfish}"

TMP_VARIANT_PATH=$(mktemp /tmp/fsx-connect-region3-XXXXXX.ini)
trap 'rm -f "${TMP_VARIANT_PATH}"' EXIT

cat >"${TMP_VARIANT_PATH}" <<'INI'
[mini-y:fairy]
maxRank = 5
maxFile = 5
hexBoard = true
pieceToCharTable = -
king = -
customPiece1 = s:m
pieceDrops = true
mustDrop = true
openingSwapDrop = true
connectPieceTypes = s
connectHorizontal = true
connectVertical = true
connectDiagonal = true
connectNorthEast = false
connectSouthEast = true
connectRegion1White = a1 b1 c1 d1 e1
connectRegion2White = a1 b2 c3 d4 e5
connectRegion3White = e1 e2 e3 e4 e5
connectRegion1Black = a1 b1 c1 d1 e1
connectRegion2Black = a1 b2 c3 d4 e5
connectRegion3Black = e1 e2 e3 e4 e5
nMoveRule = 0
startFen = ^^^^1/^^^2/^^3/^4/5[SSSSSSSSSSSSSSSsssssssssssssss] b - - 0 1
INI

run_cmds() {
  cat <<EOF | "${ENGINE}" 2>/dev/null
uci
setoption name VariantPath value ${TMP_VARIANT_PATH}
setoption name UCI_Variant value mini-y
$1
quit
EOF
}

out=$(run_cmds "position startpos
go perft 1")
echo "${out}" | grep -q "Nodes searched: 15"

out=$(run_cmds "position fen ^^^^b/^^^1b/^^2b/^3b/bbbbb w - - 0 1
go perft 1")
echo "${out}" | grep -q "Nodes searched: 0"

out=$(run_cmds "position fen ^^^^1/^^^2/^^3/^b1b1/b1b1b[S] w - - 0 1
go perft 1")
echo "${out}" | grep -q "^S@b1: 1$"

echo "connect-region3 regression passed"
