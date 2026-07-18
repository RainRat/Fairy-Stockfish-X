#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/../../../../tests/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "dots and boxes regression"
VARIANTS_MAIN=${VARIANTS}
VARIANTS_INCOMPLETE="${3:-$ROOT_DIR/src/variants-incomplete.ini}"
ENGINE_LARGE="${4:-$ROOT_DIR/src/stockfish-large}"
ENGINE_VLB="${5:-$ROOT_DIR/src/stockfish-vlb}"

echo "dots and boxes regression started"

out=$(VARIANTS="$VARIANTS_INCOMPLETE" run_perft dots-boxes-2x2 startpos 1)
assert_nodes "$out" 12

out=$(VARIANTS="$VARIANTS_MAIN" run_perft dots-boxes-7x7 startpos 1)
assert_nodes "$out" 24

if [[ -x "${ENGINE_LARGE}" ]]; then
  out=$(run_uci "$ENGINE_LARGE" "$VARIANTS_MAIN" dots-boxes-9x9 <<'EOF'
position startpos
go perft 1
EOF
)
  assert_contains_literal "$out" "info string variant dots-boxes-9x9 "
  assert_nodes "$out" 40
fi

if [[ -x "${ENGINE_VLB}" ]]; then
  out=$(run_uci "$ENGINE_VLB" "$VARIANTS_MAIN" dots-boxes-15x15 <<'EOF'
position startpos
go perft 1
EOF
)
  assert_contains_literal "$out" "info string variant dots-boxes-15x15 "
  assert_nodes "$out" 112
fi

echo "dots and boxes regression passed"
