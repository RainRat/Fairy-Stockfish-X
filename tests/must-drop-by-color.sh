#!/bin/bash
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
ENGINE="${1:-${REPO_ROOT}/src/stockfish}"
set -euo pipefail

# cd "$(dirname "$0")/../src" # removed for absolute paths

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

out_white=$(printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value asymmustdrop\nposition startpos\ngo perft 1\nquit\n' "$tmp_ini" | "$ENGINE")
# White must drop pawn; non-drop king moves should be suppressed.
grep -q "P@a" <<<"$out_white"
! grep -q "e1e2:" <<<"$out_white"

white_nodes=$(grep -o "Nodes searched: [0-9]*" <<<"$out_white" | awk '{print $3}')
if [[ -z "$white_nodes" || "$white_nodes" -le 0 ]]; then
  echo "unexpected white node count: $white_nodes"
  exit 1
fi

black_fen='4k3/8/8/8/8/8/8/4K3[p] b - - 0 1'
out_black=$(printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value asymmustdrop\nposition fen %s\ngo perft 1\nquit\n' "$tmp_ini" "$black_fen" | "$ENGINE")
# Black can either move king or drop pawn.
grep -q "e8e7:" <<<"$out_black"
grep -q "@a" <<<"$out_black"

echo "mustDropByColor test OK"