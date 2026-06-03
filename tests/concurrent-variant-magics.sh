#!/bin/bash

set -euo pipefail

error() {
  echo "concurrent-variant-magics test failed on line $1"
  [[ -n "${TMP_VARIANT_PATH:-}" ]] && rm -f "${TMP_VARIANT_PATH}"
  exit 1
}
trap 'error ${LINENO}' ERR

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "concurrent-variant-magics test"

load_inline_variants <<'INI'
[v8x8:chess]
maxFile = 8
maxRank = 8

[v6x6:chess]
maxFile = 6
maxRank = 6
INI
TMP_VARIANT_PATH="${FSX_TMP_INI}"

# We want to verify that after initializing a 6x6 variant, the 8x8 variant still works correctly
# and its magic bitboards haven't been corrupted by the global state of the 6x6 variant.
# One way to check is using 'd' command to see board representation or 'go perft' for move counts.
# For 8x8 chess startpos, perft 1 is 20.
# For 6x6 chess startpos (if it fits), let's see.

out=$(run_uci "$ENGINE" "$TMP_VARIANT_PATH" v8x8 <<'CMDS'
position startpos
go perft 1
setoption name UCI_Variant value v6x6
position startpos
go perft 1
setoption name UCI_Variant value v8x8
position startpos
go perft 1
CMDS
)

count8x8=$(echo "${out}" | grep "Nodes searched" | head -n 1 | awk '{print $3}')
count6x6=$(echo "${out}" | grep "Nodes searched" | head -n 2 | tail -n 1 | awk '{print $3}')
count8x8_again=$(echo "${out}" | grep "Nodes searched" | tail -n 1 | awk '{print $3}')

echo "v8x8 moves: ${count8x8}"
echo "v6x6 moves: ${count6x6}"
echo "v8x8 again moves: ${count8x8_again}"

if [ "${count8x8}" != "20" ]; then
    echo "Initial v8x8 perft count incorrect: ${count8x8}"
    exit 1
fi

if [ "${count8x8_again}" != "20" ]; then
    echo "Subsequent v8x8 perft count incorrect after variant switch: ${count8x8_again}"
    exit 1
fi

echo "concurrent-variant-magics tests passed"
