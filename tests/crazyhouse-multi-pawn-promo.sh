#!/bin/bash
# Regression test: captures-to-hand keeps exact promotion source piece type
# for variants with multiple promotion pawn types.

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "crazyhouse multi pawn promo"

echo "crazyhouse-multi-pawn-promo testing started"

load_inline_variants <<'VAR'
[newvariant:crazyhouse]
promotionPawnTypes=pb
promotionPieceTypes=qn
VAR
VARIANT_FILE="${FSX_TMP_INI}"

out=$(run_uci "$ENGINE" "$VARIANT_FILE" newvariant <<'EOF' 2>&1
position fen r7/7P/8/8/8/8/8/k1K5 w - - 0 1 moves h7h8q a8h8
d
EOF
)

grep -Fq "Fen: 7r/8/8/8/8/8/8/k1K5[p] w - - 0 2" <<<"$out"

echo "crazyhouse-multi-pawn-promo testing OK"
