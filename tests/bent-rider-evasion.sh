#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")/../src"

tmp_ini=$(mktemp)
trap 'rm -f "$tmp_ini"' EXIT

cat > "$tmp_ini" <<'INI'
[griffon-evasion:chess]
customPiece1 = a:O
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k

[manticore-evasion:chess]
customPiece1 = a:M
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
INI

perft_out() {
  local variant="$1"
  local fen="$2"
  printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value %s\nposition fen %s\ngo perft 1\nquit\n' "$tmp_ini" "$variant" "$fen" \
    | ./stockfish
}

# Griffon check line: f6 -> h7 (pivot g7, then outward horizontal).
# g8g7 must be legal (interposition), while g8g6 must remain illegal.
# h7h6 is also legal under the current pivot-square griffon semantics.
g=$(perft_out griffon-evasion '6r1/7k/5A2/8/8/8/8/K7 b - - 0 1')
grep -q "g8g7:" <<<"$g"
! grep -q "g8g6:" <<<"$g"

# Manticore check line: f6 -> g8 (pivot f7, then outward diagonal).
# h7f7 must be legal (interposition), while h7h6 must remain illegal.
m=$(perft_out manticore-evasion '6k1/7r/5A2/8/8/8/8/K7 b - - 0 1')
grep -q "h7f7:" <<<"$m"
! grep -q "h7h6:" <<<"$m"

echo "bent-rider-evasion test OK"
