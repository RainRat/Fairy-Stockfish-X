#!/bin/bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
ENGINE="${1:-${REPO_ROOT}/src/stockfish}"
VARIANT_PATH=${2:-src/${REPO_ROOT}/src/variants.ini}

run_cmds() {
  cat <<EOF | "${ENGINE}"
uci
setoption name VariantPath value ${VARIANT_PATH}
$1
quit
EOF
}

tmp_ini=$(mktemp)
trap 'rm -f "$tmp_ini"' EXIT

cat > "$tmp_ini" <<'INI'
[beast-pieces:chess]
customPiece1 = e:O
customPiece2 = g:NL
customPiece3 = h:M
pieceToCharTable = P..Q....EGH.Kp..q....egh.k
castling = false
doubleStep = false
promotionPieceTypes = qegh
INI

out=$(run_cmds "setoption name UCI_Variant value beast-chess
position startpos
d")
echo "${out}" | grep -q "Fen: eghqkhge/pppppppp/8/8/8/8/PPPPPPPP/EGHQKHGE w KQkq - 0 1"

out=$(cat <<EOF | "${ENGINE}"
uci
setoption name VariantPath value ${tmp_ini}
setoption name UCI_Variant value beast-pieces
position fen 4k3/8/8/8/3E4/8/8/4K3 w - - 0 1
go perft 1
quit
EOF
)
echo "${out}" | grep -q "^d4h5: 1$"
! echo "${out}" | grep -q "^d4d5:"

out=$(cat <<EOF | "${ENGINE}"
uci
setoption name VariantPath value ${tmp_ini}
setoption name UCI_Variant value beast-pieces
position fen 4k3/8/8/8/3H4/8/8/4K3 w - - 0 1
go perft 1
quit
EOF
)
echo "${out}" | grep -q "^d4g8: 1$"
! echo "${out}" | grep -q "^d4h5:"

out=$(cat <<EOF | "${ENGINE}"
uci
setoption name VariantPath value ${tmp_ini}
setoption name UCI_Variant value beast-pieces
position fen 4k3/8/8/8/3G4/8/8/4K3 w - - 0 1
go perft 1
quit
EOF
)
echo "${out}" | grep -q "^d4b5: 1$"
echo "${out}" | grep -q "^d4a5: 1$"

echo "beast-chess test OK"