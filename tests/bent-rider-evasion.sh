#!/bin/bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

ENGINE=$(default_engine "${1:-}")

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
  run_uci "$ENGINE" "$tmp_ini" "$variant" <<UCI
position fen $fen
go perft 1
UCI
}

# Griffon check line: f6 -> h7 (pivot g7, then outward horizontal).
# g8g7 must be legal (interposition), while g8g6 must remain illegal.
# h7h6 is also legal under the current pivot-square griffon semantics.
g=$(perft_out griffon-evasion '6r1/7k/5A2/8/8/8/8/K7 b - - 0 1')
assert_contains "$g" "g8g7:"
assert_not_contains "$g" "g8g6:"

# Manticore check line: f6 -> g8 (pivot f7, then outward diagonal).
# h7f7 must be legal (interposition), while h7h6 must remain illegal.
m=$(perft_out manticore-evasion '6k1/7r/5A2/8/8/8/8/K7 b - - 0 1')
assert_contains "$m" "h7f7:"
assert_not_contains "$m" "h7h6:"

echo "bent-rider-evasion test OK"
