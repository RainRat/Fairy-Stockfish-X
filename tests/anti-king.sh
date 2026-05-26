#!/bin/bash

set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ENGINE="${1:-${SCRIPT_DIR}/../src/stockfish}"
VARIANTS="${2:-${SCRIPT_DIR}/../src/variants.ini}"
source "${SCRIPT_DIR}/lib/uci.sh"

echo "anti-king tests started"

out=$(run_uci "$ENGINE" "$VARIANTS" anti-king-1 <<'EOF'
position startpos
go perft 1
EOF
)
assert_contains "$out" "^info string variant anti-king-1 "
assert_nodes "$out" 20

out=$(run_uci "$ENGINE" "$VARIANTS" anti-king-2 <<'EOF'
position startpos
go perft 1
EOF
)
assert_contains "$out" "^info string variant anti-king-2 "
assert_nodes "$out" 20
assert_contains "$out" "^d6e6: 1$"
assert_not_contains "$out" "^d6d7: 1$"

# Anti-kings may capture friendly pieces, but may not capture enemy pieces or anti-kings.
out=$(run_uci "$ENGINE" "$VARIANTS" anti-king-2 <<'EOF'
position fen 3rr2a/8/8/8/3Ap3/3P4/8/K6R w - - 0 1
go perft 1
EOF
)
assert_contains "$out" "^d4d3: 1$"
assert_not_contains "$out" "^d4e4: 1$"
assert_not_contains "$out" "^h1h8: 1$"

# Kings do not attack anti-kings, so king-only pressure leaves the anti-king side lost.
out=$(run_uci "$ENGINE" "$VARIANTS" anti-king-2 <<'EOF'
position fen 7a/8/8/3Ak3/8/8/8/K6R w - - 0 1
go perft 1
EOF
)
assert_nodes "$out" 0

# A non-king attacker restores anti-king legality.
out=$(run_uci "$ENGINE" "$VARIANTS" anti-king-2 <<'EOF'
position fen 3r3a/8/8/3Ak3/8/8/8/K6R w - - 0 1
go perft 1
EOF
)
assert_nodes "$out" 17

echo "anti-king tests passed"
