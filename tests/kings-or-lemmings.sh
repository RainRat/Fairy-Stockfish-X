#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/lib/uci.sh"

ENGINE=$(default_engine "${1:-}")
VARIANTS=$(default_variants "${2:-}")

echo "kings or lemmings regression started"

out=$(run_uci "$ENGINE" "$VARIANTS" kings-or-lemmings <<'UCI'
position fen 4k3/8/8/8/8/8/8/4K3 w - - 0 1
go perft 1
UCI
)
assert_contains "$out" "^e1e2: 1$"
assert_contains "$out" "^e1e2c: 1$"

out=$(run_uci "$ENGINE" "$VARIANTS" kings-or-lemmings <<'UCI'
position fen 4k3/8/8/8/8/8/4p3/4K3 w - - 0 1 moves e1e2c
d
UCI
)
assert_contains "$out" "Fen: 4k3/8/8/8/8/8/4K3/4K3 b - - 0 1" "restoring the king after capture"

out=$(run_uci "$ENGINE" "$VARIANTS" kings-or-lemmings <<'UCI'
position fen 8/8/8/8/8/8/8/R3K2R w KQ - 0 1
go perft 1
UCI
)
assert_contains "$out" "^e1g1: 1$"
assert_contains "$out" "^e1c1: 1$"

out=$(run_uci "$ENGINE" "$VARIANTS" kings-or-lemmings <<'UCI'
position fen 8/8/8/8/8/8/8/R3K2R w KQ - 0 1 moves e1e2c
d
UCI
)
assert_contains "$out" "Fen: 8/8/8/8/8/8/4K3/R3K2R b - - 1 1" "restoring castling-right state"

out=$(run_uci "$ENGINE" "$VARIANTS" kings-or-lemmings <<'UCI'
position fen 7k/8/8/8/8/8/rq6/K6K w - - 0 1
go depth 1
UCI
)
assert_contains_literal "$out" "bestmove (none)"

echo "kings or lemmings regression passed"
