#!/bin/bash

set -euo pipefail

ENGINE=${1:-src/stockfish}

tmp_ini=$(mktemp)
trap 'rm -f "$tmp_ini"' EXIT

cat > "$tmp_ini" <<'INI'
[nana-drop-forms:chess]
maxRank = 3
maxFile = c
pieceDrops = true
customPiece1 = a:W
customPiece2 = b:F
customPiece3 = c:D
customPiece4 = d:N
dropPieceTypes = a:abcd
dropRegionWhite = a1 b1 c1 a2 c2 a3 b3 c3
dropRegionBlack = a1 b1 c1 a2 c2 a3 b3 c3
startFen = 3/3/3[KkA] w - - 0 1
INI

out=$(cat <<EOF | "$ENGINE" 2>/dev/null
uci
setoption name VariantPath value $tmp_ini
setoption name UCI_Variant value nana-drop-forms
position startpos
go perft 1
quit
EOF
)

echo "$out" | grep -q '^A@a1: 1$'
echo "$out" | grep -q '^B@a1: 1$'
echo "$out" | grep -q '^C@a1: 1$'
echo "$out" | grep -q '^D@a1: 1$'

if echo "$out" | grep -q '@b2:'; then
  echo "center square should remain excluded for all drop forms" >&2
  exit 1
fi

if echo "$out" | grep -q '^E@'; then
  echo "unexpected unconfigured drop form generated" >&2
  exit 1
fi
