#!/usr/bin/env bash

set -euo pipefail

source "$(cd "$(dirname "$0")/../../../.." && pwd)/tests/lib/uci.sh"
setup_test_context "${1:-}" "${2:-}" "immobilizer variant"

if ! variant_available "$ENGINE" immobilizer "$VARIANTS"; then
  echo "immobilizer variant not available in this build; skipping Immobilizer regression"
  exit 0
fi

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

tmp_ini=$(mktemp)
trap 'rm -f "$tmp_ini"' EXIT
cat >"$tmp_ini" <<'INI'
[attacked-freeze:chess]
checking = false
freezePieceTypes = r
freezeAttackedSquares = true

[madrasi-freeze:attacked-freeze]
freezeSameType = true
INI

out=$(run_uci "$ENGINE" "$tmp_ini" attacked-freeze <<'UCI'
position fen 3r3B/8/8/8/8/8/3R4/K6k w - - 0 1
go perft 1
UCI
)
assert_not_contains_literal "$out" "d2" "attacked rook is immobilized"
assert_not_contains_literal "$out" "h8" "attacked bishop is immobilized"

out=$(run_uci "$ENGINE" "$tmp_ini" madrasi-freeze <<'UCI'
position fen 3r3B/8/8/8/8/8/3R4/K6k w - - 0 1
go perft 1
UCI
)
assert_not_contains_literal "$out" "d2" "Madrasi immobilizes an attacked rook of the same type"
assert_contains_literal "$out" "h8g7:" "Madrasi leaves an attacked bishop of a different type mobile"
