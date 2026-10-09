#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/../../../../tests/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "No-royal capture"

load_inline_variants <<'INI'
[noroyal-capture:chess]
king = k:K
castling = false
allowChecks = true
INI

out=$(run_uci "${ENGINE}" "${FSX_TMP_INI}" noroyal-capture <<'UCI'
position fen 4k3/8/8/8/4R3/8/8/4K3 w - - 0 1
go perft 1
UCI
)
assert_contains_literal "${out}" "e4e8: 1" "contains the royal capture"
