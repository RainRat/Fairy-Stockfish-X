#!/usr/bin/env bash

set -euo pipefail

error() {
  echo "hex chess variants regression failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

source "$(dirname "${BASH_SOURCE[0]}")/lib/uci.sh"

ENGINE="${1:-}"
if [[ -z "${ENGINE}" ]]; then
  if [[ -x "${ROOT_DIR}/src/stockfish-vlb" ]]; then
    ENGINE="${ROOT_DIR}/src/stockfish-vlb"
  else
    ENGINE=$(default_engine)
  fi
fi
VARIANT_PATH=$(default_variants "${2:-}")

variant_available() {
  local v="$1"
  local out
  out=$(run_uci "$ENGINE" "$VARIANT_PATH" "$v" <<<'d')
  grep -Fq "info string variant ${v} " <<<"${out}"
}

if ! variant_available "minihexchess" \
  || ! variant_available "glinski-chess" \
  || ! variant_available "glinski-chess-3shift" \
  || ! variant_available "glinski-chess-5shift" \
  || ! variant_available "van-gennip-hexchess" \
  || ! variant_available "van-gennip-small-hexchess" \
  || ! variant_available "mccooey-chess" \
  || ! variant_available "grand-hexachess"; then
  echo "Requires a very-large-board capable engine. Skipping."
  exit 0
fi

out=$(run_uci "$ENGINE" "$VARIANT_PATH" "minihexchess" <<<'position startpos
go perft 1')
assert_nodes "$out" 11
dump_out=$(run_uci "$ENGINE" "$VARIANT_PATH" "minihexchess" <<<'d')
assert_contains_literal "$dump_out" "startpos ***1prb/**2pkn/*3ppp/7/PPP3*/NKP2**/BRP1*** w - - 0 1"
assert_contains "$out" "^a2d3: 1$"
assert_contains "$out" "^a2b5: 1$"
assert_contains "$out" "^c1d2: 1$"
assert_contains "$out" "^a3b4: 1$"
assert_contains "$out" "^c3d4: 1$"
assert_contains "$out" "^b2d3: 1$"
assert_contains "$out" "^b2c4: 1$"

out=$(run_uci "$ENGINE" "$VARIANT_PATH" "glinski-chess" <<<'position startpos
go perft 1')
assert_nodes "$out" 40
assert_contains "$out" "^d1d2: 1$"
assert_contains "$out" "^a4b4: 1$"
assert_contains "$out" "^a1c2: 1$"
assert_contains "$out" "^a5b6: 1$"
assert_contains "$out" "^b1d2: 1$"

out=$(run_uci "$ENGINE" "$VARIANT_PATH" "glinski-chess-3shift" <<<'position startpos
go perft 1')
assert_nodes "$out" 35
assert_contains "$out" "^a2b3: 1$"
assert_contains "$out" "^a2b4: 1$"
assert_contains "$out" "^b1c2: 1$"
assert_contains "$out" "^b1d2: 1$"
assert_contains "$out" "^c5d6: 1$"

out=$(run_uci "$ENGINE" "$VARIANT_PATH" "glinski-chess-5shift" <<<'position startpos
go perft 1')
assert_nodes "$out" 37
assert_contains "$out" "^a2b3: 1$"
assert_contains "$out" "^b1c2: 1$"
assert_contains "$out" "^a5b6: 1$"
assert_contains "$out" "^b4c5: 1$"
assert_contains "$out" "^d2e3: 1$"

out=$(run_uci "$ENGINE" "$VARIANT_PATH" "van-gennip-hexchess" <<<'position startpos
go perft 1')
assert_nodes "$out" 29
assert_contains "$out" "^a2b3: 1$"
assert_contains "$out" "^c2a4: 1$"
assert_contains "$out" "^c3d4: 1$"
assert_contains "$out" "^d3e4: 1$"
assert_contains "$out" "^c2b3: 1$"
assert_contains "$out" "^e2g3: 1$"

out=$(run_uci "$ENGINE" "$VARIANT_PATH" "van-gennip-small-hexchess" <<<'position startpos
go perft 1')
assert_nodes "$out" 29
assert_contains "$out" "^c2b3: 1$"
assert_contains "$out" "^a2b3: 1$"
assert_contains "$out" "^g2h3: 1$"
assert_contains "$out" "^c3d4: 1$"
assert_contains "$out" "^f3g4: 1$"

out=$(run_uci "$ENGINE" "$VARIANT_PATH" "mccooey-chess" <<<'position startpos
go perft 1')
assert_nodes "$out" 25
assert_contains "$out" "^c3e4: 1$"
assert_contains "$out" "^c2e1: 1$"
assert_contains "$out" "^a4b5: 1$"

out=$(run_uci "$ENGINE" "$VARIANT_PATH" "grand-hexachess" <<<'position startpos
go perft 1')
assert_nodes "$out" 125
assert_contains "$out" "^i13g12: 1$"
assert_contains "$out" "^a5a6: 1$"
assert_contains "$out" "^k5k6: 1$"
assert_contains "$out" "^c3d4: 1$"
assert_contains "$out" "^e11f10: 1$"
assert_contains "$out" "^j13k12: 1$"

echo "hex chess variants regression passed"
