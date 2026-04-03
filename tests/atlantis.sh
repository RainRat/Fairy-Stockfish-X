#!/bin/bash

set -euo pipefail

error() {
  echo "atlantis regression failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-"$ENGINE"}
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
VARIANTS=${2:-${REPO_ROOT}/src/variants.ini}

run_cmds() {
  cat <<EOF | "${ENGINE}" 2>/dev/null
uci
setoption name VariantPath value ${VARIANTS}
setoption name UCI_Variant value atlantis
$1
quit
EOF
}

out=$(run_cmds "position startpos
go perft 1")
echo "${out}" | grep -q "^a2a3: 1$"
! echo "${out}" | grep -q "^a2a3,a1: 1$"
echo "${out}" | grep -q "^0000,a3: 1$"

out=$(run_cmds "position startpos moves a2a3
d")
echo "${out}" | grep -q "Fen: rnbqkbnr/pppppppp/8/8/8/P7/1PPPPPPP/RNBQKBNR b KQkq - 0 1"

out=$(run_cmds "position startpos moves 0000,a3
d")
echo "${out}" | grep -q "Fen: rnbqkbnr/pppppppp/8/8/8/\\*7/PPPPPPPP/RNBQKBNR b KQkq - 1 1"
