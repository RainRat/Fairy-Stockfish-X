#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ENGINE="${1:-${SCRIPT_DIR}/../src/stockfish}"
VARIANTS="${2:-${SCRIPT_DIR}/../src/variants.ini}"
source "${SCRIPT_DIR}/lib/uci.sh"

echo "hindustani regression started"

out=$(run_uci "$ENGINE" "$VARIANTS" hindustani <<'EOF'
position startpos
go perft 1
EOF
)
assert_contains "$out" "^e1d3: 1$"
assert_contains "$out" "^e1f3: 1$"

out=$(run_uci "$ENGINE" "$VARIANTS" hindustani <<'EOF'
position startpos moves e1d3
go perft 1
EOF
)
assert_not_contains "$out" "^d3b2: 1$"
assert_not_contains "$out" "^d3f2: 1$"

out=$(run_uci "$ENGINE" "$VARIANTS" hindustani <<'EOF'
position fen 3k4/8/8/8/8/8/r7/4K3 b E - 0 1 moves a2e2
go perft 1
EOF
)
assert_not_contains "$out" "^e1d3: 1$"
assert_not_contains "$out" "^e1f3: 1$"

echo "hindustani regression passed"
