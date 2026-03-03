#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")/../src"

tmp_ini=$(mktemp)
trap 'rm -f "$tmp_ini"' EXIT

cat > "$tmp_ini" <<'INI'
[promhandgate:chess]
captureType = hand
pieceDrops = true
pocketSize = 5
promotionPieceTypes = q
promotionRequireInHand = true
startFen = 4k3/6P1/8/8/8/8/8/4K3[] w - - 0 1

[promhandok:chess]
captureType = hand
pieceDrops = true
pocketSize = 5
promotionPieceTypes = q
promotionRequireInHand = true
startFen = 4k3/6P1/8/8/8/8/8/4K3[Q] w - - 0 1
INI

run_perft() {
  local variant=$1
  printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value %s\nposition startpos\ngo perft 1\nquit\n' "$tmp_ini" "$variant" | ./stockfish
}

out_gate=$(run_perft promhandgate)
out_ok=$(run_perft promhandok)

grep -q "Nodes searched: 5" <<<"$out_gate"
! grep -q "g7g8q:" <<<"$out_gate"

grep -q "g7g8q:" <<<"$out_ok"

echo "promotionRequireInHand test OK"
