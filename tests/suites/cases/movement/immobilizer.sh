#!/usr/bin/env bash

set -euo pipefail

source "$(cd "$(dirname "$0")/../../../.." && pwd)/tests/lib/uci.sh"
setup_test_context "${1:-}" "${2:-}" "immobilizer variant"

out=$(run_uci "$ENGINE" "$VARIANTS" immobilizer <<'UCI'
position startpos
go perft 1
UCI
)
assert_contains_literal "$out" "Nodes searched:" "Immobilizer start position loads"

out=$(run_uci "$ENGINE" "$VARIANTS" immobilizer <<'UCI'
position fen 4k5/10/10/10/10/3i6/3R6/10/10/4K5 w - - 0 1
go perft 1
UCI
)
assert_not_contains_literal "$out" "d4" "adjacent enemy piece is immobilized"

out=$(run_uci "$ENGINE" "$VARIANTS" immobilizer <<'UCI'
position fen 4k5/10/10/10/3r6/3I6/10/10/10/4K5 w - - 0 1
go perft 1
UCI
)
assert_not_contains_literal "$out" "d5d6" "immobilizer cannot capture"
assert_contains_literal "$out" "d5d4:" "immobilizer retains quiet queen moves"
