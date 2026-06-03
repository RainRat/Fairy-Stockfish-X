#!/bin/bash

set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"
init_test_env "${1:-}" "${2:-}" "variant-switch-after-perft regression"

load_inline_variants <<'INI'
[v1:chess]
startFen = 4k3/8/8/8/8/8/8/4K3 w - - 0 1

[v2:v1]
startFen = 4k3/8/8/8/4P3/8/8/4K3 w - - 0 1
INI
TMP_VARIANT_PATH="${FSX_TMP_INI}"

out=$(run_uci "$ENGINE" "$TMP_VARIANT_PATH" v1 <<'CMDS'
position startpos
go perft 1
setoption name UCI_Variant value v2
CMDS
)

assert_contains "$out" "^e1d1: 1$"
assert_contains "$out" "info string variant v2 files 8 ranks 8 pocket 0 template fairy startpos 4k3/8/8/8/4P3/8/8/4K3 w - - 0 1"

echo "variant-switch-after-perft regression tests passed"
