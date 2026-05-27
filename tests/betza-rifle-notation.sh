#!/bin/bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

ENGINE=$(default_engine "${1:-}")

tmp_ini="$(mktemp)"
trap 'rm -f "$tmp_ini"' EXIT

cat >"$tmp_ini" <<'EOF'
[betzarifle:chess]
customPiece1 = a:R^

[betzaplain:chess]
customPiece1 = a:R
EOF

run_d() {
  local variant="$1"
  local moves="$2"
  run_uci "$ENGINE" "$tmp_ini" "$variant" <<UCI
position fen p3k3/8/8/8/8/8/8/A3K3 w - - 0 1${moves}
d
UCI
}

rifle_moves=$(run_uci "$ENGINE" "$tmp_ini" betzarifle <<'UCI'
position fen p3k3/8/8/8/8/8/8/A3K3 w - - 0 1
go perft 1
UCI
)
assert_contains "$rifle_moves" "^a1a8: 1$"

plain_after="$(run_d "betzaplain" " moves a1a8")"
assert_contains_literal "$plain_after" "Fen: A3k3/8/8/8/8/8/8/4K3 b - - 0 1"

rifle_after="$(run_d "betzarifle" " moves a1a8")"
assert_contains_literal "$rifle_after" "Fen: 4k3/8/8/8/8/8/8/A3K3 b - - 0 1"

echo "betza rifle notation passed"
