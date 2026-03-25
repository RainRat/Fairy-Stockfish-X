#!/bin/bash

set -euo pipefail

error() {
  echo "dead-pieces test failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-}
if [[ -z "${ENGINE}" ]]; then
  if [[ -x "src/stockfish" ]]; then
    ENGINE="src/stockfish"
  else
    ENGINE="./stockfish"
  fi
fi

run_cmds() {
  cat <<EOF | "${ENGINE}" 2>/dev/null
uci
setoption name VariantPath value src/variants.ini
$1
quit
EOF
}

# A death-on-capture piece becomes a ^ dead square after capturing.
out=$(run_cmds "setoption name UCI_Variant value fatal-giveaway
position fen 4k3/8/8/4p3/4R3/8/8/4K3 w - - 0 1 moves e4e5
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/4\\^3/8/8/8/4K3 b - - 0 1"

# Dead squares are capturable by either side.
out=$(run_cmds "setoption name UCI_Variant value fatal-giveaway
position fen 4k3/8/8/4\\^3/3P4/8/8/4K3 w - - 0 1
go perft 1")
echo "${out}" | grep -q "^d4e5: 1$"

# Dead squares are immobile blockers; they do not generate moves of their own.
out=$(run_cmds "setoption name UCI_Variant value fatal-giveaway
position fen 4k3/8/8/4\\^3/8/8/8/4K3 b - - 0 1
go perft 1")
! echo "${out}" | grep -q "^e5"
