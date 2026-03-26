#!/bin/bash

set -euo pipefail

error() {
  echo "capture-to-hand-types regression failed on line $1" >&2
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-./stockfish}

TMP_INI=$(mktemp)
trap 'rm -f "${TMP_INI}"' EXIT

cat > "${TMP_INI}" <<'INI'
[capture-to-hand-types-demo:fairy]
maxFile = g
maxRank = 7
king = k
rook = r
knight = n
pieceDrops = true
captureType = hand
captureToHandTypes = r
promotionPieceTypes = -
doubleStep = false
castling = false
checking = true
INI

run_cmds() {
  cat <<EOF | "${ENGINE}" 2>/dev/null
uci
setoption name VariantPath value ${TMP_INI}
setoption name UCI_Variant value capture-to-hand-types-demo
$1
quit
EOF
}

# Capturing a rook should add it to hand because rook is in captureToHandTypes.
out=$(run_cmds "position fen r3k2/7/7/7/7/7/R3K2 w - - 0 1 moves a1a7
d")
echo "${out}" | grep -q "Fen: R3k2/7/7/7/7/7/4K2\\[R\\] b - - 0 1"

# Capturing a knight should not add it to hand because knight is excluded.
out=$(run_cmds "position fen n3k2/7/7/7/7/7/R3K2 w - - 0 1 moves a1a7
d")
echo "${out}" | grep -q "Fen: R3k2/7/7/7/7/7/4K2\\[\\] b - - 0 1"
