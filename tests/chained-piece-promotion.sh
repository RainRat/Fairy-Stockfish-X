#!/bin/bash

set -euo pipefail

error() {
  echo "chained-piece-promotion test failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-}
if [[ -z "${ENGINE}" ]]; then
  if [[ -x "src/stockfish" ]]; then
    ENGINE="src/stockfish"
  else
    ENGINE="./stockfish"
  fi
fi

tmp_ini=$(mktemp)
trap 'rm -f "${tmp_ini}"' EXIT

cat > "${tmp_ini}" <<'INI'
[chainbug:chess]
pawn = -
customPiece1 = p:mW
customPiece2 = n:N
customPiece3 = b:B
pieceToCharTable = ...PNB...................pnb...................
pawnType = p
promotionRegionWhite = *1 *2 *3 *4 *5 *6 *7 *8
promotionRegionBlack = *1 *2 *3 *4 *5 *6 *7 *8
mandatoryPiecePromotion = false
promotedPieceType = p:n n:b
checking = false
INI

run_cmds() {
  cat <<EOF | "${ENGINE}" 2>/dev/null
uci
setoption name VariantPath value ${tmp_ini}
setoption name UCI_Variant value chainbug
$1
quit
EOF
}

# Native unpromoted knight can still move onto the promotion region and remain a knight.
out=$(run_cmds "position fen 4k3/8/8/8/8/4N3/8/4K3 w - - 0 1 moves e3d1
d")
echo "${out}" | grep -q "^Fen: 4k3/8/8/8/8/8/8/3NK3 b - - 1 1$"

# A promoted pawn that became a knight must not be promoted again when moving in the region.
out=$(run_cmds "position fen 4k3/8/8/8/8/4+P3/8/4K3 w - - 0 1 moves e3d1
d")
echo "${out}" | grep -q "^Fen: 4k3/8/8/8/8/8/8/3+PK3 b - - 1 1$"
