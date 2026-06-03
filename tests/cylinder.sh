#!/bin/bash

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "cylinder test"

out=$(run_perft cylinder "4k3/8/8/8/8/8/8/R3K3 w - - 0 1" 1)
echo "${out}" | grep -q "^a1h1: 1$"

out=$(run_perft cylinder "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1" 1)
! echo "${out}" | grep -q "^e1g1: 1$"
! echo "${out}" | grep -q "^e1c1: 1$"

out=$(run_perft cylinder-castling "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1" 1)
echo "${out}" | grep -q "^e1g1: 1$"
echo "${out}" | grep -q "^e1c1: 1$"

echo "cylinder test OK"
