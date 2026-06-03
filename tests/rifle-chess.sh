#!/bin/bash

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "rifle chess test"

extract_fen() {
  sed -n 's/^Fen: //p' | tail -n1
}

echo "rifle chess tests started"

# Capture removes the target but the shooter stays on its square.
out=$(run_uci "$ENGINE" "$VARIANTS" rifle-chess <<'EOF'
position fen 4k3/8/8/8/8/8/4q3/3QK3 w - - 0 1 moves d1e2
d
EOF
)
fen=$(echo "${out}" | extract_fen)
[[ "${fen}" == "4k3/8/8/8/8/8/8/3QK3 b - - 0 1" ]]

# Shooting a blocker can give check while the rook remains on its origin square.
out=$(run_uci "$ENGINE" "$VARIANTS" rifle-chess <<'EOF'
position fen k7/n7/8/8/8/8/8/R3K3 w - - 0 1 moves a1a7
d
EOF
)
fen=$(echo "${out}" | extract_fen)
[[ "${fen}" == "k7/8/8/8/8/8/8/R3K3 b - - 0 1" ]]
echo "${out}" | grep -q "^Checkers: a1 "

# Capturing from the promotion zone is still a normal shot, not a promotion move.
out=$(run_uci "$ENGINE" "$VARIANTS" rifle-chess <<'EOF'
position fen 3rk3/4P3/8/8/8/8/8/4K3 w - - 0 1
go perft 1
EOF
)
echo "${out}" | grep -q "^e7d8: 1$"
! echo "${out}" | grep -q "^e7d8[qnbr]:"

echo "rifle chess tests passed"
