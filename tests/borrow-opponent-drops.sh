#!/usr/bin/env bash
set -euo pipefail

error() {
  echo "borrow-opponent-drops regression failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE="${1:-./src/stockfish}"

TMP_VARIANT_PATH=$(mktemp /tmp/fsx-borrow-drops-XXXXXX.ini)
trap 'rm -f "${TMP_VARIANT_PATH}"' EXIT

cat >"${TMP_VARIANT_PATH}" <<'INI'
[borrow-slide:fairy]
maxRank = 5
maxFile = e
pieceToCharTable = -
king = -
customPiece1 = a:-
pieceDrops = true
mustDrop = true
captureType = hand
captureToHandSide = owner
borrowOpponentDropsWhenEmpty = true
edgeInsertOnly = true
dropRegion = a* *5
edgeInsertTypes = a
edgeInsertRegion = a* *5
edgeInsertFrom = top left
pushingStrength = a:5
startFen = 5/5/5/5/5[a] w - - 0 1
INI

run_cmds() {
  cat <<EOF | "${ENGINE}" 2>/dev/null
uci
setoption name VariantPath value ${TMP_VARIANT_PATH}
setoption name UCI_Variant value borrow-slide
$1
quit
EOF
}

out=$(run_cmds "position startpos
go perft 1")
echo "${out}" | grep -q "^A@a1,b1: 1$"

out=$(run_cmds "position startpos moves A@a1,b1
d")
echo "${out}" | grep -Fq "Fen: 5/5/5/5/a4[] b - - 0 1"

echo "borrow-opponent-drops regression passed"
