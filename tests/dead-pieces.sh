#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "dead-pieces regression"

out=$(run_uci "$ENGINE" "$VARIANTS" fatal-giveaway <<'UCI'
position fen 4k3/8/8/4p3/4R3/8/8/4K3 w - - 0 1 moves e4e5
d
UCI
)
assert_contains_literal "$out" "Fen: 4k3/8/8/4^3/8/8/8/4K3 b - - 0 1"

out=$(run_uci "$ENGINE" "$VARIANTS" fatal-giveaway <<'UCI'
position fen 4k3/8/8/4^3/3P4/8/8/4K3 w - - 0 1
go perft 1
UCI
)
assert_contains "$out" "^d4e5: 1$"

out=$(run_uci "$ENGINE" "$VARIANTS" fatal-giveaway <<'UCI'
position fen 4k3/8/8/4^3/8/8/8/4K3 b - - 0 1
go perft 1
UCI
)
assert_not_contains "$out" "^e5"
