#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ENGINE="${1:-${SCRIPT_DIR}/../src/stockfish}"
VARIANT_PATH=${2:-}
source "${SCRIPT_DIR}/lib/uci.sh"

tmp_ini=
cleanup() {
  if [[ -n "${tmp_ini}" ]]; then
    rm -f "${tmp_ini}"
  fi
}
trap cleanup EXIT

if [[ -z "${VARIANT_PATH}" ]]; then
  tmp_ini=$(mktemp)
  cat > "${tmp_ini}" <<'EOF'
[battery-chess:chess]
captureType = hand
pieceDrops = false
promotionRequireInHand = true
promotionConsumeInHand = true
EOF
  VARIANT_PATH="${tmp_ini}"
fi

echo "battery-chess test started"

out=$(run_uci "$ENGINE" "$VARIANT_PATH" battery-chess <<'EOF'
position fen 4k3/P7/8/8/8/8/8/4K3 w - - 0 1
go perft 1
EOF
)
assert_not_contains "$out" "^a7a8"

out=$(run_uci "$ENGINE" "$VARIANT_PATH" battery-chess <<'EOF'
position fen 4k3/P7/8/8/8/8/8/4K3[Q] w - - 0 1
go perft 1
EOF
)
assert_contains "$out" "^a7a8q: 1$"
assert_not_contains "$out" "^a7a8n:"
assert_not_contains "$out" "^a7a8r:"
assert_not_contains "$out" "^a7a8b:"

out=$(run_uci "$ENGINE" "$VARIANT_PATH" battery-chess <<'EOF'
position fen 4k3/P7/8/8/8/8/8/4K3[Q] w - - 0 1 moves a7a8q
d
EOF
)
assert_contains "$out" "Fen: Q~3k3/8/8/8/8/8/8/4K3\\[\\] b - - 0 1"

echo "battery-chess test OK"
