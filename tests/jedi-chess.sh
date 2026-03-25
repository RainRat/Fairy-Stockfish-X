#!/bin/bash

set -euo pipefail

error() {
  echo "jedi-chess regression failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-./stockfish}
VARIANTS=${2:-/home/chris/Fairy-Stockfish-X/src/variants.ini}

run_cmds() {
  cat <<EOF | "${ENGINE}" 2>/dev/null
uci
setoption name VariantPath value ${VARIANTS}
setoption name UCI_Variant value jedi-chess
$1
quit
EOF
}

# Black Sith Master may capture its own Apprentice.
out=$(run_cmds "position fen 4s3/4q3/8/8/8/8/8/4K3 b - - 0 1
go perft 1")
echo "${out}" | grep -q "^e8e7: 1$"

# If the Sith Master is gone, the side loses immediately.
out=$(run_cmds "position fen 8/4q3/8/8/8/8/8/4K3 w - - 0 1
go depth 1")
echo "${out}" | grep -q "^bestmove (none)$"
