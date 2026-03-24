#!/bin/bash

set -euo pipefail

error() {
  echo "gadsden-toroidal test failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-./stockfish}
VARIANT_PATH=${2:-src/variants.ini}

run_cmds() {
  cat <<EOF | "${ENGINE}" 2>/dev/null
uci
setoption name VariantPath value ${VARIANT_PATH}
$1
quit
EOF
}

out=$(run_cmds "setoption name UCI_Variant value gadsden-toroidal
position startpos
d")
echo "${out}" | grep -q "Fen: pppppppp/rnbqkbnr/pppppppp/8/8/PPPPPPPP/RNBQKBNR/PPPPPPPP w - - 0 1"

out=$(run_cmds "setoption name UCI_Variant value gadsden-toroidal
position fen 2k5/8/p7/7P/8/4K3/8/8 w - - 0 1
go perft 1")
echo "${out}" | grep -q "^h5a6: 1$"

out=$(run_cmds "setoption name UCI_Variant value gadsden-toroidal
position fen 8/P7/2k5/8/8/4K3/8/8 w - - 0 1
go perft 1")
echo "${out}" | grep -q "^a7a8q: 1$"
! echo "${out}" | grep -q "^a7a8: 1$"

out=$(run_cmds "setoption name UCI_Variant value gadsden-toroidal-wrap-pawns
position fen 8/P7/2k5/8/8/4K3/8/8 w - - 0 1
go perft 1")
echo "${out}" | grep -q "^a7a8: 1$"
! echo "${out}" | grep -q "^a7a8q: 1$"

out=$(run_cmds "setoption name UCI_Variant value gadsden-toroidal-wrap-pawns
position fen P7/8/2k5/8/8/4K3/8/8 w - - 0 1
go perft 1")
echo "${out}" | grep -q "^a8a1: 1$"

echo "gadsden-toroidal test OK"
