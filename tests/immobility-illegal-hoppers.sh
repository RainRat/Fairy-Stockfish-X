#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")/../src"

tmp_ini=$(mktemp)
trap 'rm -f "$tmp_ini"' EXIT

cat > "$tmp_ini" <<'INI'
[immobility-illegal-hopper-test:shogi]
customPiece1 = m:fpR
customPiece2 = j:fC
promotedPieceType = p:g m:g j:g s:g b:h r:d
startFen = 2sgkgs2/1r5b1/p1ppppp1p/1p5p1/9/1P5P1/P1PPPPP1P/1B5R1/2SGKGS2[mmMMjjJJ]
INI

out=$(printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value immobility-illegal-hopper-test\nposition fen 9/9/9/9/9/9/9/9/4K4[MJJjjmm] w - - 0 1\ngo perft 1\nquit\n' "$tmp_ini" | ./stockfish)

grep -q '^M@a7:' <<<"$out"
grep -q '^M@e7:' <<<"$out"
! grep -q '^M@a8:' <<<"$out"
! grep -q '^M@e8:' <<<"$out"
! grep -q '^M@a9:' <<<"$out"
! grep -q '^M@e9:' <<<"$out"

echo "immobility-illegal hoppers regression passed"
