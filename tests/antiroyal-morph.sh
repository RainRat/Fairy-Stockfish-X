#!/bin/bash

set -euo pipefail

error() {
  echo "antiroyal-morph test failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-./src/stockfish}
TEMP_INI=$(mktemp)

cat <<EOF > "${TEMP_INI}"
[antiroyal-morph:chess]
antiRoyalTypes = Q
moveMorphPieceType = n:q
EOF

# White has only one Queen (d1), which is NOT attacked. White IS in check.
# White moves Knight g1 to f3, which morphs into a second Queen.
# Since count(Q) > 1, they are no longer anti-royal.
# The move g1f3 should be legal.
# We also have an attacked Black Queen on c8 to keep Black out of check.
# White Rook on h8 attacks Black Queen c8.

out=$(cat <<CMDS | "${ENGINE}"
uci
setoption name VariantPath value ${TEMP_INI}
setoption name UCI_Variant value antiroyal-morph
position fen 2q1k2R/8/8/8/8/8/8/3QK1N1 w - - 0 1
go perft 1
quit
CMDS
)

rm "${TEMP_INI}"

if ! echo "${out}" | grep -q "g1f3: 1"; then
  echo "antiroyal-morph test failed: g1f3 not found in perft 1"
  exit 1
fi

echo "antiroyal-morph test passed"
