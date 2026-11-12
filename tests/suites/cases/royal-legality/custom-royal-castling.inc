#!/usr/bin/env bash
# Custom royals castle like kings: a royal castler must not pass through
# attack, matching the transit checks physical kings get. Non-royal castlers
# deliberately keep the exemption (see the royalLikeCastler comment).

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/../../../../tests/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "custom royal castling"

echo "custom royal castling tests started"

# separate-realms has a custom royal (k) with castling enabled. The black c
# attacks f1, the transit square of white kingside castling.
out=$(run_uci "$ENGINE" "$VARIANTS" separate-realms <<'UCI'
position fen 4k3/8/8/8/2c5/8/8/4K2E w K - 0 1
go perft 1
UCI
)
assert_not_contains "$out" "^e1g1: 1$" "royal must not castle through check"
assert_nodes "$out" 8

# Without the attack, the same castling is legal.
out=$(run_uci "$ENGINE" "$VARIANTS" separate-realms <<'UCI'
position fen 4k3/8/8/8/8/8/8/4K2E w K - 0 1
go perft 1
UCI
)
assert_contains "$out" "^e1g1: 1$" "unattacked royal castling remains"
assert_nodes "$out" 9

echo "custom royal castling tests passed"
