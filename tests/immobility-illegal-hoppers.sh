#!/usr/bin/env bash

set -euo pipefail

engine=${1:-src/stockfish}

tmp_ini=$(mktemp)
trap 'rm -f "$tmp_ini"' EXIT

cat > "$tmp_ini" <<'EOF'
[hopperdrop:chess]
pieceToCharTable = -
king = -
customPiece1 = m:fpR
customPiece2 = l:fmRfcpR
pieceDrops = true
mustDrop = true
dropRegionWhite = **
dropRegionBlack = **
immobilityIllegal = true
startFen = 8/8/8/8/8/8/8/8[ML] w - - 0 1
EOF

run_perft() {
  local variant=$1
  printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value %s\nposition startpos\ngo perft 1\nquit\n' \
    "$tmp_ini" "$variant" | "$engine"
}

out=$(run_perft hopperdrop)

if grep -q '^M@[a-h]7: 1$' <<<"$out"; then
  echo "pure forward hopper incorrectly allowed to drop on the penultimate rank"
  exit 1
fi

if grep -q '^M@[a-h]8: 1$' <<<"$out"; then
  echo "pure forward hopper incorrectly allowed to drop on the last rank"
  exit 1
fi

if ! grep -q '^L@[a-h]7: 1$' <<<"$out"; then
  echo "mixed lance-like hopper unexpectedly lost penultimate-rank drops"
  exit 1
fi

if grep -q '^L@[a-h]8: 1$' <<<"$out"; then
  echo "mixed lance-like hopper incorrectly allowed to drop on the last rank"
  exit 1
fi

echo "immobility-illegal-hoppers test OK"
