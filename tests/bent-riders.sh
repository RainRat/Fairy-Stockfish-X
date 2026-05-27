#!/bin/bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

ENGINE=$(default_engine "${1:-}")

tmp_ini=$(mktemp)
trap 'rm -f "$tmp_ini"' EXIT

cat > "$tmp_ini" <<'INI'
[griffon-test:chess]
customPiece1 = a:O
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 7k/8/8/8/3A4/8/8/K7 w - - 0 1

[manticore-test:chess]
customPiece1 = a:M
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 7k/8/8/8/3A4/8/8/K7 w - - 0 1
INI

perft_out() {
  local variant="$1"
  run_uci "$ENGINE" "$tmp_ini" "$variant" <<'UCI'
position startpos
go perft 1
UCI
}

g=$(perft_out griffon-test)
assert_contains "$g" "d4h5:"
assert_contains "$g" "d4a5:"
assert_contains "$g" "d4e8:"
assert_contains "$g" "d4c1:"
assert_not_contains "$g" "d4d5:"
assert_not_contains "$g" "d4e4:"

m=$(perft_out manticore-test)
assert_contains "$m" "d4g8:"
assert_contains "$m" "d4a6:"
assert_contains "$m" "d4h1:"
assert_contains "$m" "d4b1:"
assert_not_contains "$m" "d4h5:"
assert_not_contains "$m" "d4e8:"

echo "bent-riders test OK"
