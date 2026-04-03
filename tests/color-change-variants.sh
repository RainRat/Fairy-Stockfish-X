#!/bin/bash

set -euo pipefail

error() {
  echo "color-change variants test failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
ENGINE="${1:-${REPO_ROOT}/src/stockfish}"

run_cmds() {
  local variant="$1"
  local cmds="$2"
  cat <<CMDS | "${ENGINE}"
uci
setoption name UCI_Variant value ${variant}
${cmds}
quit
CMDS
}

extract_fen() {
  sed -n 's/^Fen: //p' | tail -n1
}

echo "color-change variant tests started"

out=$(run_cmds "antiandernach" "position fen 4k3/8/8/4P3/8/8/8/4K3 w - - 0 1 moves e5e6
d")
fen=$(echo "${out}" | extract_fen)
[[ "${fen}" == "4k3/8/4p3/8/8/8/8/4K3 b - - 0 1" ]]

out=$(run_cmds "andernach" "position fen 4k3/8/3n4/4P3/8/8/8/4K3 w - - 0 1 moves e5d6
d")
fen=$(echo "${out}" | extract_fen)
[[ "${fen}" == "4k3/8/3p4/8/8/8/8/4K3 b - - 0 1" ]]

out=$(run_cmds "superandernach" "position fen 4k3/8/8/4P3/8/8/8/4K3 w - - 0 1 moves e5e6
d")
fen=$(echo "${out}" | extract_fen)
[[ "${fen}" == "4k3/8/4p3/8/8/8/8/4K3 b - - 0 1" ]]

out=$(run_cmds "antiandernach" "position fen k7/8/8/8/8/8/4R3/4K3 w - - 0 1
go perft 1")
! echo "${out}" | grep -q "^e2e7:"

out=$(run_cmds "recycle" "position fen 6k1/8/8/5N2/4P3/8/8/6K1[] w - - 17 1 moves e4f5
d")
fen=$(echo "${out}" | extract_fen)
[[ "${fen}" == "6k1/8/8/5P2/8/8/8/6K1[N] b - - 0 1" ]]

echo "color-change variant tests passed"