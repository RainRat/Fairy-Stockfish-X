#!/bin/bash

set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "rose regression"

load_inline_variants <<'INI'
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
TMP_VARIANT_PATH="${FSX_TMP_INI}"

perft_out() {
  local variant="$1"
  run_uci "$ENGINE" "$TMP_VARIANT_PATH" "$variant" <<'CMDS'
position startpos
go perft 1
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

echo "rose regression tests passed"
