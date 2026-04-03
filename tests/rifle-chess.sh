#!/bin/bash

set -euo pipefail

error() {
  echo "rifle chess test failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
ENGINE="${1:-${REPO_ROOT}/src/stockfish}"
VARIANT_PATH=${2:-src/${REPO_ROOT}/src/variants.ini}

run_cmds() {
  local cmds="$1"
  cat <<CMDS | "${ENGINE}"
uci
setoption name VariantPath value ${VARIANT_PATH}
setoption name UCI_Variant value rifle-chess
${cmds}
quit
CMDS
}

extract_fen() {
  sed -n 's/^Fen: //p' | tail -n1
}

echo "rifle chess tests started"

# Capture removes the target but the shooter stays on its square.
out=$(run_cmds "position fen 4k3/8/8/8/8/8/4q3/3QK3 w - - 0 1 moves d1e2
d")
fen=$(echo "${out}" | extract_fen)
[[ "${fen}" == "4k3/8/8/8/8/8/8/3QK3 b - - 0 1" ]]

# Shooting a blocker can give check while the rook remains on its origin square.
out=$(run_cmds "position fen k7/n7/8/8/8/8/8/R3K3 w - - 0 1 moves a1a7
d")
fen=$(echo "${out}" | extract_fen)
[[ "${fen}" == "k7/8/8/8/8/8/8/R3K3 b - - 0 1" ]]
echo "${out}" | grep -q "^Checkers: a1 "

# Capturing from the promotion zone is still a normal shot, not a promotion move.
out=$(run_cmds "position fen 3rk3/4P3/8/8/8/8/8/4K3 w - - 0 1
go perft 1")
echo "${out}" | grep -q "^e7d8: 1$"
! echo "${out}" | grep -q "^e7d8[qnbr]:"

echo "rifle chess tests passed"