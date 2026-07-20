#!/bin/bash

set -euo pipefail

ENGINE=${1:-./stockfish}
VARIANTS=${2:-src/variants.ini}

score() {
  local fen=$1
  cat <<CMDS | "${ENGINE}" | awk '/^Final evaluation/ { print $3 }' | tail -n1
uci
setoption name VariantPath value ${VARIANTS}
setoption name UCI_Variant value checkers
position fen ${fen}
eval
quit
CMDS
}

two_men=$(score '8/8/7m/8/8/8/8/M1M5 w - - 0 1')
man_vs_king=$(score '8/8/7k/8/8/8/8/M7 w - - 0 1')

awk -v value="${two_men}" 'BEGIN { exit !(value > 0.2) }'
awk -v value="${man_vs_king}" 'BEGIN { exit !(value < -0.2) }'

echo "checkers evaluation tests passed"
