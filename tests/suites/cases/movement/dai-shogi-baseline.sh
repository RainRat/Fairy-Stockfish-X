#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/../../../../tests/lib/uci.sh"

ENGINE=$(default_engine "${1:-}")
VARIANTS=${2:-${ROOT_DIR}/src/variants.ini}

if [[ ! -x "${ENGINE}" && "${FSX_ALLOW_SMALL_BOARD:-0}" == 1 ]]; then
  fsx_test_skip "VLB engine unavailable at ${ENGINE}; skip allowed by FSX_ALLOW_SMALL_BOARD=1"
fi

# Reviewed baseline after removing inline Betza comments from the shipped pieces.
output=$(run_uci "${ENGINE}" "${VARIANTS}" dai_shogi <<'UCI'
position startpos
go perft 1
UCI
)
assert_contains "${output}" "variant dai_shogi files 15 ranks 15"
assert_nodes "${output}" 71

echo "dai_shogi movement baseline passed"
