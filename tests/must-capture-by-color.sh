#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "must capture by color regression"

load_inline_variants <<'INI'
[asymmustcapture:chess]
mustCaptureWhite = true
mustCaptureBlack = false
startFen = 4k3/8/8/3p4/4P3/8/8/4K3 w - - 0 1
INI
tmp_ini="${FSX_TMP_INI}"

out_white=$(run_uci "$ENGINE" "$tmp_ini" asymmustcapture <<'UCI'
position startpos
go perft 1
UCI
)
assert_contains "$out_white" "e4d5:"
assert_not_contains "$out_white" "e4e5:"

assert_nodes "$out_white" 1

black_fen='4k3/8/8/4p3/3P4/8/8/4K3 b - - 0 1'
out_black=$(run_uci "$ENGINE" "$tmp_ini" asymmustcapture <<UCI
position fen ${black_fen}
go perft 1
UCI
)
assert_contains "$out_black" "e5d4:"
assert_contains "$out_black" "e5e4:"

echo "mustCaptureByColor test OK"
