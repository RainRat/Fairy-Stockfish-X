#!/bin/bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

ENGINE=$(default_engine "${1:-}")
VARIANTS=$(default_variants "${2:-}")

echo "atlantis tests started"

out=$(run_uci "$ENGINE" "$VARIANTS" atlantis <<'EOF'
position startpos
go perft 1
EOF
)
assert_contains "$out" "^a2a3: 1$"
assert_not_contains "$out" "^a2a3,a1: 1$"
assert_contains "$out" "^0000,a3: 1$"

out=$(run_uci "$ENGINE" "$VARIANTS" atlantis <<'EOF'
position startpos moves a2a3
d
EOF
)
assert_contains_literal "$out" "Fen: rnbqkbnr/pppppppp/8/8/8/P7/1PPPPPPP/RNBQKBNR b KQkq - 0 1"

out=$(run_uci "$ENGINE" "$VARIANTS" atlantis <<'EOF'
position startpos moves 0000,a3
d
EOF
)
assert_contains_literal "$out" "Fen: rnbqkbnr/pppppppp/8/8/8/*7/PPPPPPPP/RNBQKBNR b KQkq - 1 1"

echo "atlantis tests passed"
