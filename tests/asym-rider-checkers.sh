#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")/../src"

tmp_ini=$(mktemp)
trap 'rm -f "$tmp_ini"' EXIT

cat > "$tmp_ini" <<'INI'
[asymcheck-horse:chess]
customPiece1 = a:nN
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k

[asymcheck-griffon:chess]
customPiece1 = a:O
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k

[asymcheck-manticore:chess]
customPiece1 = a:M
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
INI

diag() {
  local variant=$1
  local fen=$2
  printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value %s\nposition fen %s\nd\nquit\n' "$tmp_ini" "$variant" "$fen" \
    | ./stockfish
}

# Horse-family blocked leg: no check.
hb=$(diag asymcheck-horse '4k3/3R4/3A4/8/8/8/8/4K3 b - - 0 1')
grep -q '^Checkers:[[:space:]]*$' <<<"$hb"

# Horse-family open leg: checker on d6.
hu=$(diag asymcheck-horse '4k3/8/3A4/8/8/8/8/4K3 b - - 0 1')
grep -q '^Checkers: d6 ' <<<"$hu"

# Griffon blocked pivot: no check.
gb=$(diag asymcheck-griffon '8/5P1k/5A2/8/8/8/8/4K3 b - - 0 1')
grep -q '^Checkers:[[:space:]]*$' <<<"$gb"

# Griffon open pivot/ray: checker on f6.
gu=$(diag asymcheck-griffon '8/7k/5A2/8/8/8/8/4K3 b - - 0 1')
grep -q '^Checkers: f6 ' <<<"$gu"

# Manticore blocked pivot: no check.
mb=$(diag asymcheck-manticore '6k1/6N1/5A2/8/8/8/8/4K3 b - - 0 1')
grep -q '^Checkers:[[:space:]]*$' <<<"$mb"

# Manticore open pivot/ray: checker on f6.
mu=$(diag asymcheck-manticore '6k1/8/5A2/8/8/8/8/4K3 b - - 0 1')
grep -q '^Checkers: f6 ' <<<"$mu"

echo "asym-rider-checkers test OK"
