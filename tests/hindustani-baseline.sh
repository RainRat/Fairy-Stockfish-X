#!/bin/bash

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "hindustani baseline test"

run_pyffish_test <<'PY'
import os
import pyffish as sf

cfg = open(os.path.join(os.environ["ROOT_DIR"], "src", "variants.ini"), encoding="utf-8").read()
sf.load_variant_config(cfg)

cases = [
    ("7k/8/8/8/8/8/8/7K w - - 0 1", 0),
    ("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1", -sf.VALUE_MATE),
]

for fen, expected in cases:
    result = sf.game_result("hindustani", fen, [])
    if result != expected:
        raise SystemExit(f"unexpected Hindustani result for {fen}: got {result}, expected {expected}")
PY

out=$(run_perft "hindustani" "4k3/P7/8/8/8/8/8/4K3 w - - 0 1" 1)
echo "${out}" | grep -q "^a7a8r: 1$"
! echo "${out}" | grep -q "^a7a8q: 1$"

out=$(run_perft "hindustani" "4k3/2P5/8/8/8/8/8/4K3 w - - 0 1" 1)
echo "${out}" | grep -q "^c7c8x: 1$"
! echo "${out}" | grep -q "^c7c8b: 1$"

out=$(run_perft "hindustani" "4k3/4P3/8/8/8/8/8/4K3 w - - 0 1" 1)
! echo "${out}" | grep -q "^e7e8"

out=$(run_perft "hindustani" "4k3/2P5/8/8/8/8/8/2X1K3 w - - 0 1" 1)
! echo "${out}" | grep -q "^c7c8x: 1$"

out=$(run_perft "hindustani" "startpos" 1)
echo "${out}" | grep -q "^e1d3: 1$"
echo "${out}" | grep -q "^e1f3: 1$"

display_out=$(run_display "hindustani" "startpos")
assert_contains_literal "$display_out" "Fen: rnxqkynr/pppppppp/8/8/8/8/PPPPPPPP/RNXQKYNR w Ed - 0 1"

echo "hindustani baseline test passed"
