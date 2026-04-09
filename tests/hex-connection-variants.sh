#!/usr/bin/env bash
set -euo pipefail

error() {
  echo "hex connection variants regression failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE="${1:-}"
if [[ -z "${ENGINE}" ]]; then
  if [[ -x "src/stockfish-vlb" ]]; then
    ENGINE="src/stockfish-vlb"
  elif [[ -x "./src/stockfish-vlb" ]]; then
    ENGINE="./src/stockfish-vlb"
  else
    ENGINE="./src/stockfish"
  fi
fi
VARIANT_PATH="${2:-./src/variants.ini}"

run_cmds() {
  cat <<EOF | "${ENGINE}" 2>/dev/null
uci
setoption name VariantPath value ${VARIANT_PATH}
setoption name UCI_Variant value $1
$2
quit
EOF
}

variant_available() {
  local v="$1"
  local out
  out=$(run_cmds "${v}" "d")
  echo "${out}" | grep -q "info string variant ${v} "
}

if ! variant_available "hex"; then
  echo "hex connection variants regression requires a very-large-board capable engine"
  exit 1
fi

out=$(run_cmds "hex" "position startpos
go perft 1")
echo "${out}" | grep -q "Nodes searched: 121"

out=$(run_cmds "hex" "position fen 11/11/11/11/11/11/11/11/11/11/PPPPPPPPPPP b - - 0 1
go perft 1")
echo "${out}" | grep -q "Nodes searched: 0"

out=$(run_cmds "y" "position startpos
go perft 1")
echo "${out}" | grep -q "Nodes searched: 55"

echo "hex connection variants regression passed"
