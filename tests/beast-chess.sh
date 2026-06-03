#!/bin/bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ENGINE="${1:-${SCRIPT_DIR}/../src/stockfish}"
VARIANT_PATH="${2:-${SCRIPT_DIR}/../src/variants.ini}"
source "${SCRIPT_DIR}/lib/uci.sh"

tmp_ini=$(mktemp)
fsx_add_exit_cleanup 'rm -f "$tmp_ini"'

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

if ! variant_available "$ENGINE" beast-chess "$VARIANT_PATH"; then
  echo "beast-chess variant not available in this build; skipping beast-chess regression"
  exit 0
fi

out=$(run_uci "$ENGINE" "$VARIANT_PATH" beast-chess <<'EOF'
position startpos
d
EOF
)
assert_contains "$out" "Fen: eghqkhge/pppppppp/8/8/8/8/PPPPPPPP/EGHQKHGE w KQkq - 0 1"

out=$(run_uci "$ENGINE" "$tmp_ini" beast-pieces <<'EOF'
position fen 4k3/8/8/8/3E4/8/8/4K3 w - - 0 1
go perft 1
EOF
)
assert_contains "$out" "^d4h5: 1$"
assert_not_contains "$out" "^d4d5:"

out=$(run_uci "$ENGINE" "$tmp_ini" beast-pieces <<'EOF'
position fen 4k3/8/8/8/3H4/8/8/4K3 w - - 0 1
go perft 1
EOF
)
assert_contains "$out" "^d4g8: 1$"
assert_not_contains "$out" "^d4h5:"

out=$(run_uci "$ENGINE" "$tmp_ini" beast-pieces <<'EOF'
position fen 4k3/8/8/8/3G4/8/8/4K3 w - - 0 1
go perft 1
EOF
)
assert_contains "$out" "^d4b5: 1$"
assert_contains "$out" "^d4a5: 1$"

echo "beast-chess test OK"
