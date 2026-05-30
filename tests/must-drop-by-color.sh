#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/lib/uci.sh"

ENGINE=$(default_engine "${1:-}")

tmp_ini=$(mktemp)
trap 'rm -f "$tmp_ini"' EXIT

cat > "$tmp_ini" <<'INI'
[asymmustdrop:chess]
pieceDrops = true
mustDrop = false
mustDropWhite = true
mustDropBlack = false
mustDropTypeWhite = p
startFen = 4k3/8/8/8/8/8/8/4K3[P] w - - 0 1
INI

out_white=$(run_uci_cmds "$ENGINE" "$tmp_ini" asymmustdrop "position startpos
go perft 1")
# White must drop pawn; non-drop king moves should be suppressed.
assert_contains "$out_white" "P@a"
assert_not_contains "$out_white" "e1e2:"

white_nodes=$(grep -o "Nodes searched: [0-9]*" <<<"$out_white" | awk '{print $3}')
if [[ -z "$white_nodes" || "$white_nodes" -le 0 ]]; then
  echo "unexpected white node count: $white_nodes"
  exit 1
fi

black_fen='4k3/8/8/8/8/8/8/4K3[p] b - - 0 1'
out_black=$(run_uci_cmds "$ENGINE" "$tmp_ini" asymmustdrop "position fen ${black_fen}
go perft 1")
# Black can either move king or drop pawn.
assert_contains "$out_black" "e8e7:"
assert_contains "$out_black" "@a"

echo "mustDropByColor test OK"
