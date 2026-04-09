#!/usr/bin/env bash

set -euo pipefail

error() {
  echo "hex chess variants regression failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-src/stockfish-vlb}
VARIANT_PATH=${2:-src/variants.ini}

run_cmds() {
  cat <<EOF | "${ENGINE}" 2>/dev/null
uci
setoption name VariantPath value ${VARIANT_PATH}
$1
quit
EOF
}

variant_available() {
  local v="$1"
  local out
  out=$(run_cmds "setoption name UCI_Variant value ${v}
d")
  echo "${out}" | grep -q "info string variant ${v} "
}

if ! variant_available "minihexchess" \
  || ! variant_available "glinski-chess" \
  || ! variant_available "glinski-chess-3shift" \
  || ! variant_available "glinski-chess-5shift" \
  || ! variant_available "van-gennip-hexchess" \
  || ! variant_available "van-gennip-small-hexchess" \
  || ! variant_available "mccooey-chess" \
  || ! variant_available "grand-hexachess"; then
  echo "Requires a very-large-board capable engine. Skipping."
  exit 0
fi

out=$(run_cmds "setoption name UCI_Variant value minihexchess
position startpos
go perft 1")
echo "${out}" | grep -q "Nodes searched: 6"
dump_out=$(run_cmds "setoption name UCI_Variant value minihexchess
d")
echo "${dump_out}" | grep -q "startpos \\*\\*\\*1prb/\\*\\*2pkn/\\*3ppp/7/PPP3\\*/NKP2\\*\\*/BRP1\\*\\*\\* w - - 0 1"
echo "${out}" | grep -q "^a2b4: 1$"
echo "${out}" | grep -q "^a3a4: 1$"
echo "${out}" | grep -q "^b3b4: 1$"
echo "${out}" | grep -q "^c3c4: 1$"
echo "${out}" | grep -q "^b2d3: 1$"
echo "${out}" | grep -q "^b2c4: 1$"

out=$(run_cmds "setoption name UCI_Variant value glinski-chess
position startpos
go perft 1")
echo "${out}" | grep -q "Nodes searched: 34"
echo "${out}" | grep -q "^d1d2: 1$"
echo "${out}" | grep -q "^a4b4: 1$"
echo "${out}" | grep -q "^a1c2: 1$"
echo "${out}" | grep -q "^a5a6: 1$"
echo "${out}" | grep -q "^b1d2: 1$"

out=$(run_cmds "setoption name UCI_Variant value glinski-chess-3shift
position startpos
go perft 1")
echo "${out}" | grep -q "Nodes searched: 26"
echo "${out}" | grep -q "^a2b3: 1$"
echo "${out}" | grep -q "^a2b4: 1$"
echo "${out}" | grep -q "^b1c2: 1$"
echo "${out}" | grep -q "^b1d2: 1$"
echo "${out}" | grep -q "^c5c6: 1$"

out=$(run_cmds "setoption name UCI_Variant value glinski-chess-5shift
position startpos
go perft 1")
echo "${out}" | grep -q "Nodes searched: 19"
echo "${out}" | grep -q "^a2b3: 1$"
echo "${out}" | grep -q "^b1c2: 1$"
echo "${out}" | grep -q "^a5a6: 1$"
echo "${out}" | grep -q "^b5b6: 1$"
echo "${out}" | grep -q "^b5c5: 1$"

out=$(run_cmds "setoption name UCI_Variant value van-gennip-hexchess
position startpos
go perft 1")
echo "${out}" | grep -q "Nodes searched: 16"
echo "${out}" | grep -q "^a2a3: 1$"
echo "${out}" | grep -q "^b2b3: 1$"
echo "${out}" | grep -q "^g2g3: 1$"
echo "${out}" | grep -q "^c3b3: 1$"
echo "${out}" | grep -q "^c3c4: 1$"
echo "${out}" | grep -q "^c2b3: 1$"
echo "${out}" | grep -q "^e2g3: 1$"

out=$(run_cmds "setoption name UCI_Variant value van-gennip-small-hexchess
position startpos
go perft 1")
echo "${out}" | grep -q "Nodes searched: 16"
echo "${out}" | grep -q "^c2b3: 1$"
echo "${out}" | grep -q "^a2a3: 1$"
echo "${out}" | grep -q "^g2g3: 1$"
echo "${out}" | grep -q "^c3c4: 1$"
echo "${out}" | grep -q "^f3g3: 1$"

out=$(run_cmds "setoption name UCI_Variant value mccooey-chess
position startpos
go perft 1")
echo "${out}" | grep -q "Nodes searched: 10"
echo "${out}" | grep -q "^c3e4: 1$"
echo "${out}" | grep -q "^c2e1: 1$"
echo "${out}" | grep -q "^a4a5: 1$"

out=$(run_cmds "setoption name UCI_Variant value grand-hexachess
position startpos
go perft 1")
echo "${out}" | grep -q "Nodes searched: 125"
echo "${out}" | grep -q "^i13g12: 1$"
echo "${out}" | grep -q "^a5a6: 1$"
echo "${out}" | grep -q "^k5k6: 1$"
echo "${out}" | grep -q "^c3d4: 1$"
echo "${out}" | grep -q "^e11f10: 1$"
echo "${out}" | grep -q "^j13k12: 1$"

echo "hex chess variants regression passed"
