#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ENGINE="${1:-${SCRIPT_DIR}/../src/stockfish}"
VARIANTS="${2:-${SCRIPT_DIR}/../src/variants.ini}"
source "${SCRIPT_DIR}/lib/uci.sh"

run_cmds() {
  run_uci "$ENGINE" "$VARIANTS" haynie-leapers <<EOF
$1
EOF
}

echo "haynie leapers regression tests started"

if ! variant_available "$ENGINE" haynie-leapers "$VARIANTS"; then
  echo "haynie-leapers variant not available in this build; skipping haynie-leapers regression"
  exit 0
fi

out=$(run_cmds "setoption name UCI_Variant value haynie-leapers
position startpos
go perft 1")
assert_contains "$out" "^Nodes searched: 28$"
assert_contains "$out" "^a1c4: 1$"
assert_contains "$out" "^c1b3: 1$"
assert_contains "$out" "^b1a4: 1$"

out=$(run_cmds "setoption name UCI_Variant value haynie-leapers
position fen k7/7P/8/8/8/8/8/7K w - - 0 1
go perft 1")
assert_contains "$out" "^h7h8z: 1$"
assert_contains "$out" "^h7h8c: 1$"
assert_contains "$out" "^h7h8w: 1$"
assert_not_contains "$out" "^h7h8: 1$"

echo "haynie leapers regression tests passed"
