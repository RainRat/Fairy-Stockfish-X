#!/bin/bash

set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ENGINE="${1:-${SCRIPT_DIR}/../src/stockfish}"
VARIANT_PATH="${2:-${SCRIPT_DIR}/../src/variants.ini}"
source "${SCRIPT_DIR}/lib/uci.sh"

echo "constabulary-chess test started"

if ! variant_available "$ENGINE" constabulary-chess "$VARIANT_PATH"; then
  echo "constabulary-chess variant not available in this build; skipping constabulary-chess regression"
  exit 0
fi

out=$(run_uci "$ENGINE" "$VARIANT_PATH" constabulary-chess <<'EOF'
position startpos
d
EOF
)
assert_contains "$out" "Fen: wxeiiexw/rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR/WXEIIEXW w KQkq - 0 1"

out=$(run_uci "$ENGINE" "$VARIANT_PATH" constabulary-chess <<'EOF'
position fen 8/8/8/8/8/8/8/8/R3K2R/8 w KQ - 0 1 moves e2g2
d
EOF
)
assert_contains "$out" "Fen: 8/8/8/8/8/8/8/8/R4RK1/8 b - - 1 1"

out=$(run_uci "$ENGINE" "$VARIANT_PATH" constabulary-chess <<'EOF'
position fen 7k/P7/8/8/8/8/8/8/8/7K w - - 0 1
go perft 1
EOF
)
assert_contains "$out" "^a9a10q: 1$"
assert_contains "$out" "^a9a10w: 1$"
assert_contains "$out" "^a9a10x: 1$"
assert_contains "$out" "^a9a10e: 1$"
assert_contains "$out" "^a9a10i: 1$"

echo "constabulary-chess test OK"
