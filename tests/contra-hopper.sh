#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")/../src"

tmp_ini=$(mktemp)
trap 'rm -f "$tmp_ini"' EXIT

cat > "$tmp_ini" <<'INI'
[contrahopper:chess]
customPiece1 = a:oR
startFen = 4k3/8/3p4/8/3A1p2/8/3p4/K7 w - - 0 1
INI

out=$(printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value contrahopper\nposition startpos\ngo perft 1\nquit\n' "$tmp_ini" | ./stockfish)

grep -q "d4d5:" <<<"$out"
grep -q "d4e4:" <<<"$out"
grep -q "d4d3:" <<<"$out"
! grep -q "d4d6:" <<<"$out"
! grep -q "d4f4:" <<<"$out"

echo "contra-hopper test OK"
