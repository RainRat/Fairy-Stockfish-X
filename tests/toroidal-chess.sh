#!/bin/bash

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "toroidal-chess test"

out=$(run_display toroidal-chess startpos)
echo "${out}" | grep -q "Fen: r1b2b1r/pp4pp/n1pqkp1n/3pp3/3PP3/N1PQKP1N/PP4PP/R1B2B1R w - - 0 1"

out=$(run_perft toroidal-chess "1k6/8/8/8/8/8/4K3/R7 w - - 0 1" 1)
echo "${out}" | grep -q "^a1h1: 1$"
echo "${out}" | grep -q "^a1a8: 1$"

out=$(run_perft toroidal-chess "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1" 1)
! echo "${out}" | grep -q "^e1g1: 1$"
! echo "${out}" | grep -q "^e1c1: 1$"

echo "toroidal-chess test OK"
