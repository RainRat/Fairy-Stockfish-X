#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "kopano regression"
VARIANT_PATH="${VARIANT_PATH:-${VARIANTS}}"

out=$(run_uci "$ENGINE" "$VARIANT_PATH" kopano <<'EOF'
position startpos
go perft 1
EOF
)
assert_nodes "$out" 64

out=$(run_uci "$ENGINE" "$VARIANT_PATH" kopano <<'EOF'
position startpos moves P@b1
go perft 1
EOF
)
assert_contains "$out" "^P@a2: 1$"
assert_not_contains "$out" "^P@b1: 1$"

out=$(run_uci "$ENGINE" "$VARIANT_PATH" kopano <<'EOF'
position fen 8/8/8/8/8/8/1P6/8[Pp] w - - 0 1
go perft 1
EOF
)
assert_not_contains "$out" "^P@c3: 1$"

out=$(run_uci "$ENGINE" "$VARIANT_PATH" kopano <<'EOF'
position fen 8/8/8/8/3p4/8/1P6/8[Pp] w - - 0 1
go perft 1
EOF
)
assert_contains "$out" "^P@c3: 1$"

out=$(run_uci "$ENGINE" "$VARIANT_PATH" kopano <<'EOF'
position fen 8/8/8/8/2pP4/3p4/8/8[Pp] w - - 0 1
go perft 1
EOF
)
assert_not_contains "$out" "^P@c3: 1$"

out=$(run_uci "$ENGINE" "$VARIANT_PATH" kopano <<'EOF'
position fen 7p/6p1/5p2/4p3/3p4/2p5/1p6/p7 w - - 0 1
go perft 1
EOF
)
assert_nodes "$out" 0

echo "kopano regression passed"
