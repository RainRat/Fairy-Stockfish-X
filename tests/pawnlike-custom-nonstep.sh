#!/bin/bash

set -euo pipefail

error() {
  echo "pawn-like custom non-step test failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-./stockfish}

tmp_ini=$(mktemp)
trap 'rm -f "$tmp_ini"' EXIT

cat > "$tmp_ini" <<'INI'
[pawnlike-nonstep:chess]
customPiece1 = d:NN
pieceToCharTable = PNBRQ............D...Kpnbrq............d...k
pawnLikeTypes = d
startFen = 4k3/8/8/8/8/8/D7/K7 w - - 0 1
INI

out=$(printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value pawnlike-nonstep\nposition startpos\ngo perft 1\nquit\n' "$tmp_ini" \
  | "$ENGINE")

echo "$out" | grep -q "^a2c1: 1$"
echo "$out" | grep -q "^a2c3: 1$"
if echo "$out" | grep -Eq "^a2a[34]:"; then
  echo "custom pawn-like non-step piece received generic pawn push"
  exit 1
fi

echo "pawn-like custom non-step tests passed"
