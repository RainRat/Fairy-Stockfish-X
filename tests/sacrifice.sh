#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ENGINE="${1:-${SCRIPT_DIR}/../src/stockfish}"
VARIANTS="${2:-${SCRIPT_DIR}/../src/variants.ini}"
source "${SCRIPT_DIR}/lib/uci.sh"

echo "sacrifice regression started"

out=$(run_uci "$ENGINE" "$VARIANTS" sacrifice <<'EOF'
position startpos
go perft 1
EOF
)
assert_contains "$out" "^h2h2x: 1$"
assert_contains "$out" "^a2a2x: 1$"
assert_not_contains "$out" "^g1g1x: 1$"

echo "sacrifice regression passed"
