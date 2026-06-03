#!/bin/bash

set -euo pipefail

error() {
  echo "bombardment test failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ENGINE="${1:-${SCRIPT_DIR}/../src/stockfish}"
VARIANT_PATH="${2:-${SCRIPT_DIR}/../src/variants.ini}"
source "${SCRIPT_DIR}/lib/uci.sh"

run_cmds() {
  run_uci "$ENGINE" "$VARIANT_PATH" bombardment <<EOF
$1
EOF
}

variant_available() {
  local out
  out=$(printf 'uci\nquit\n' | uci_timeout "$ENGINE")
  grep -q ' var bombardment ' <<<"$out"
}

if ! variant_available; then
  echo "bombardment variant not available in this build; skipping bombardment test"
  exit 0
fi

out=$(run_cmds "setoption name UCI_Variant value bombardment
position startpos
go perft 1")
assert_contains "$out" "^a2a3: 1$"
assert_contains "$out" "^a2b3: 1$"
assert_contains "$out" "^a2a2x: 1$"
assert_not_contains "$out" "^a2b2:"

out=$(run_cmds "setoption name UCI_Variant value bombardment
position startpos moves a2a3
d")
assert_contains "$out" "Fen: mmmmmmmm/mmmmmmmm/8/8/8/M7/1MMMMMMM/MMMMMMMM b - - 1 1"

out=$(run_cmds "setoption name UCI_Variant value bombardment
position fen 8/8/2mmm3/2mMm3/2mmm3/8/8/M7 w - - 0 1 moves d5d5x
d")
assert_contains "$out" "Fen: 8/8/8/8/8/8/8/M7 b - - 1 1"

echo "bombardment ok"
