#!/bin/bash

set -euo pipefail

error() {
  echo "hippolyta regression failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-./stockfish}
VARIANTS=${2:-src/variants.ini}

run_cmds() {
  cat <<EOF | "${ENGINE}" 2>/dev/null
uci
setoption name VariantPath value ${VARIANTS}
setoption name UCI_Variant value hippolyta
$1
quit
EOF
}

out=$(run_cmds "position startpos
go perft 1")
echo "${out}" | grep -q "^a1b2: 1$"
if echo "${out}" | grep -q "^a1a2:"; then
  echo "hippolyta generated illegal quiet move"
  exit 1
fi

out=$(run_cmds "position startpos moves a1b2
d")
echo "${out}" | grep -q "Fen: aaaaaaaa/AAAAAAAa/AaaaaaAa/AaAAAaAa/AaAaaaAa/AaAAAAAa/A1aaaaaa/AAAAAAAA b - - 0 1"
