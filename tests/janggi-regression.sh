#!/bin/bash

set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "janggi regression"

if ! variant_available "$ENGINE" janggi "$VARIANTS"; then
  echo "janggi variant not available in this build; skipping janggi regression"
  exit 0
fi

out=$(run_uci "$ENGINE" "$VARIANTS" janggi <<'EOF'
position startpos
go perft 1
EOF
)
assert_contains "${out}" "^Nodes searched: 32$"
assert_contains "${out}" "^0000: 1$"

out=$(run_uci "$ENGINE" "$VARIANTS" janggi <<'EOF'
position fen 1n1kaabn1/cr2N4/5C1c1/p1pNp3p/9/9/P1PbP1P1P/3r1p3/4A4/R1BA1KB1R b - - 0 1 moves a9e9 e2d3
go perft 1
EOF
)
assert_contains "${out}" "^Nodes searched: 37$"
assert_contains "${out}" "^f3e2: 1$"
assert_contains "${out}" "^0000: 1$"

echo "janggi regression tests passed"
