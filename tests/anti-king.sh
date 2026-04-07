#!/bin/bash

set -euo pipefail

error() {
  echo "anti-king test failed on line $1" >&2
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-./src/stockfish}
VARIANTS=${2:-/home/chris/Fairy-Stockfish-X/src/variants.ini}

run_engine() {
  local variant="$1"
  local position_cmd="$2"
  cat <<EOF | "${ENGINE}" 2>/dev/null
uci
setoption name VariantPath value ${VARIANTS}
setoption name UCI_Variant value ${variant}
${position_cmd}
go perft 1
quit
EOF
}

echo "anti-king tests started"

out=$(run_engine anti-king-1 "position startpos")
echo "${out}" | grep -q "^info string variant anti-king-1 "
echo "${out}" | grep -q "^Nodes searched: 20$"

out=$(run_engine anti-king-2 "position startpos")
echo "${out}" | grep -q "^info string variant anti-king-2 "
echo "${out}" | grep -q "^Nodes searched: 20$"
echo "${out}" | grep -q "^d6e6: 1$"
! echo "${out}" | grep -q "^d6d7: 1$"

# Anti-kings may capture friendly pieces, but may not capture enemy pieces or anti-kings.
out=$(run_engine anti-king-2 "position fen 3rr2a/8/8/8/3Ap3/3P4/8/K6R w - - 0 1")
echo "${out}" | grep -q "^d4d3: 1$"
! echo "${out}" | grep -q "^d4e4: 1$"
! echo "${out}" | grep -q "^h1h8: 1$"

# Kings do not attack anti-kings, so king-only pressure leaves the anti-king side lost.
out=$(run_engine anti-king-2 "position fen 7a/8/8/3Ak3/8/8/8/K6R w - - 0 1")
echo "${out}" | grep -q "^Nodes searched: 0$"

# A non-king attacker restores anti-king legality.
out=$(run_engine anti-king-2 "position fen 3r3a/8/8/3Ak3/8/8/8/K6R w - - 0 1")
echo "${out}" | grep -q "^Nodes searched: 17$"

echo "anti-king tests passed"
