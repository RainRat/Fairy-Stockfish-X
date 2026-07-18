#!/usr/bin/env bash

set -euo pipefail

source "$(cd "$(dirname "$0")/../../../.." && pwd)/tests/lib/uci.sh"
setup_test_context "${1:-}" "${2:-}" "promotion/drop rule matrix"

custom_variants=$(mktemp "${TMPDIR:-/tmp}/fsx-drop-matrix-XXXXXX.ini")
trap 'rm -f "$custom_variants"' EXIT
cat >"$custom_variants" <<'INI'
[drop-immediate:fairy]
pieceDrops = true
dropPieceTypes = q
nMoveRuleImmediate = 1
nMoveRule = 0
startFen = 4k3/8/8/8/8/8/8/4K3[Q] w - - 1 1
INI

out=$(run_uci "$ENGINE" "$custom_variants" drop-immediate <<'UCI'
position startpos moves Q@e7
d
UCI
)
assert_contains_literal "$out" "Fen: 4k3/4Q3/8/8/8/8/8/4K3" "drop is applied before immediate-end adjudication"
assert_contains_literal "$out" " b " "drop changes the side to move"

echo "promotion/drop rule matrix cases passed"
