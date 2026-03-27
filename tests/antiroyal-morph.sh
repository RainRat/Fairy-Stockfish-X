#!/bin/bash

set -euo pipefail

error() {
  echo "antiroyal-morph test failed on line $1" >&2
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-./src/stockfish}
TMP_INI=$(mktemp)
trap 'rm -f "${TMP_INI}"' EXIT

cat > "${TMP_INI}" <<'EOF'
[antiroyal-morph:chess]
antiRoyalTypes = Q
moveMorphPieceType = n:q

[antiroyal-capturemorph:chess]
antiRoyalTypes = Q
captureMorph = true
checking = false
EOF

out=$(cat <<EOF | "${ENGINE}" 2>/dev/null
uci
setoption name VariantPath value ${TMP_INI}
setoption name UCI_Variant value antiroyal-morph
position fen 2q1k2R/8/8/8/8/8/8/3QK1N1 w - - 0 1
go perft 1
quit
EOF
)

echo "${out}" | grep -q "^g1f3: 1$"

out=$(cat <<EOF | "${ENGINE}" 2>/dev/null
uci
setoption name VariantPath value ${TMP_INI}
setoption name UCI_Variant value antiroyal-capturemorph
position fen q3k3/8/8/8/8/8/4q3/3QK1N1 w - - 0 1
go perft 1
quit
EOF
)

echo "${out}" | grep -q "^g1e2: 1$"

echo "antiroyal-morph test passed"
