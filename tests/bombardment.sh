#!/bin/bash

set -euo pipefail

error() {
  echo "bombardment test failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
ENGINE="${1:-${REPO_ROOT}/src/stockfish}"
if [[ -z "${ENGINE}" ]]; then
  if [[ -x "src/stockfish" ]]; then
    ENGINE="src/stockfish"
  else
    ENGINE=""$ENGINE""
  fi
fi
VARIANT_PATH=${2:-${REPO_ROOT}/src/variants.ini}
if [[ ! -f "${VARIANT_PATH}" && -f "src/${REPO_ROOT}/src/variants.ini" ]]; then
  VARIANT_PATH="src/${REPO_ROOT}/src/variants.ini"
fi

run_cmds() {
  cat <<EOF | "${ENGINE}" 2>/dev/null
uci
setoption name VariantPath value ${VARIANT_PATH}
$1
quit
EOF
}

out=$(run_cmds "setoption name UCI_Variant value bombardment
position startpos
go perft 1")
echo "${out}" | grep -q "^a2a3: 1$"
echo "${out}" | grep -q "^a2b3: 1$"
echo "${out}" | grep -q "^a2a2x: 1$"
! echo "${out}" | grep -q "^a2b2:"

out=$(run_cmds "setoption name UCI_Variant value bombardment
position startpos moves a2a3
d")
echo "${out}" | grep -q "Fen: mmmmmmmm/mmmmmmmm/8/8/8/M7/1MMMMMMM/MMMMMMMM b - - 1 1"

out=$(run_cmds "setoption name UCI_Variant value bombardment
position fen 8/8/2mmm3/2mMm3/2mmm3/8/8/M7 w - - 0 1 moves d5d5x
d")
echo "${out}" | grep -q "Fen: 8/8/8/8/8/8/8/M7 b - - 0 1"

echo "bombardment ok"