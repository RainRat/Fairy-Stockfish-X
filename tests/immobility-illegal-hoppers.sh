#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")/../src"

tmp_ini=$(mktemp)
trap 'rm -f "$tmp_ini"' EXIT

cat > "$tmp_ini" <<'INI'
[immobility-illegal-hopper-test:chess]
maxFile = i
maxRank = 9
pieceDrops = true
immobilityIllegal = true
king = k:W
customPiece1 = m:fpR
customPiece2 = g:W
promotedPieceType = m:g
startFen = 9/9/9/9/9/9/9/9/4K4[M]
INI

out=$(printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value immobility-illegal-hopper-test\nposition fen 9/9/9/9/9/9/9/9/4K4[M] w - - 0 1\ngo perft 1\nquit\n' "$tmp_ini" | ./stockfish)

grep -q '^M@a7:' <<<"$out"
grep -q '^M@e7:' <<<"$out"
! grep -q '^M@a8:' <<<"$out"
! grep -q '^M@e8:' <<<"$out"
! grep -q '^M@a9:' <<<"$out"
! grep -q '^M@e9:' <<<"$out"

echo "immobility-illegal hoppers regression passed"
