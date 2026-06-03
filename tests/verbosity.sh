#!/bin/bash

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "verbosity regression"

uci_output=$(run_uci "$ENGINE" "$VARIANTS" chess <<'EOF'
EOF
)
assert_contains "$uci_output" 'option name Verbosity type spin default 1 min 0 max 2'

quiet_output=$(run_uci "$ENGINE" "$VARIANTS" chess <<'EOF'
setoption name Verbosity value 0
position startpos
go depth 2
EOF
)
if grep -q '^info depth ' <<<"$quiet_output"; then
  echo "Verbosity=0 unexpectedly emitted search info"
  exit 1
fi

debug_output=$(run_uci "$ENGINE" "$VARIANTS" chess <<'EOF'
setoption name Verbosity value 2
position fen 7k/5Q2/7K/8/8/8/8/8 b - - 0 1
go depth 1
EOF
)
assert_contains "$debug_output" 'info string adjudication reason stalemate result cp 0 side_to_move black'

echo "verbosity regression passed"
