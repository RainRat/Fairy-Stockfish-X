#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "whaleshogi regression"

out_start=$(run_uci "$ENGINE" "$VARIANTS" whaleshogi <<'UCI'
position startpos
go perft 1
UCI
)
assert_nodes "$out_start" 7

out_promo=$(run_uci "$ENGINE" "$VARIANTS" whaleshogi <<'UCI'
position fen 5w/4D1/6/6/6/W5 w - - 0 1
go perft 1
UCI
)
assert_contains_literal "$out_promo" "e5e6+:"
assert_not_contains_literal "$out_promo" "e5e6:"

out_demote=$(run_uci "$ENGINE" "$VARIANTS" whaleshogi <<'UCI'
position fen 4+Dw/6/6/6/6/W5 w - - 0 1
go perft 1
UCI
)
assert_contains_literal "$out_demote" "e6d5-:"
assert_contains_literal "$out_demote" "e6f5-:"
assert_not_contains_literal "$out_demote" "e6d5:"

echo "whaleshogi regression passed"
