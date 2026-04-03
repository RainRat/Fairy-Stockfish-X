#!/bin/bash

set -euo pipefail

error() {
  echo "rose regression failed on line $1"
  [[ -n "${TMP_VARIANT_PATH:-}" ]] && rm -f "${TMP_VARIANT_PATH}"
  exit 1
}
trap 'error ${LINENO}' ERR

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
ENGINE="${1:-${REPO_ROOT}/src/stockfish}"

TMP_VARIANT_PATH=$(mktemp /tmp/fsx-rose-XXXXXX.ini)
cat >"${TMP_VARIANT_PATH}" <<'INI'
[rose-empty:chess]
king = -
checking = false
customPiece1 = a:rose
pieceToCharTable = A:a
startFen = 8/8/8/8/8/8/8/A7 w - - 0 1

[rose-block-b3:rose-empty]
startFen = 8/8/8/8/8/1p6/8/A7 w - - 0 1

[rose-block-c2:rose-empty]
startFen = 8/8/8/8/8/8/2p5/A7 w - - 0 1

[rose-block-both:rose-empty]
startFen = 8/8/8/8/8/1p6/2p5/A7 w - - 0 1
INI

perft_out() {
  local variant="$1"
  cat <<CMDS | "${ENGINE}"
uci
setoption name VariantPath value ${TMP_VARIANT_PATH}
setoption name UCI_Variant value ${variant}
position startpos
go perft 1
quit
CMDS
}

echo "rose regression tests started"

empty=$(perft_out rose-empty)
echo "${empty}" | grep -q "^a1b3: 1$"
echo "${empty}" | grep -q "^a1c2: 1$"
echo "${empty}" | grep -q "^a1d4: 1$"
echo "${empty}" | grep -q "^a1e1: 1$"

block_b3=$(perft_out rose-block-b3)
echo "${block_b3}" | grep -q "^a1d4: 1$"

block_c2=$(perft_out rose-block-c2)
echo "${block_c2}" | grep -q "^a1d4: 1$"

block_both=$(perft_out rose-block-both)
! echo "${block_both}" | grep -q "^a1d4: 1$"
echo "${block_both}" | grep -q "^a1b3: 1$"
echo "${block_both}" | grep -q "^a1c2: 1$"

rm -f "${TMP_VARIANT_PATH}"
unset TMP_VARIANT_PATH

echo "rose regression tests passed"