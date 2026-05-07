#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ENGINE="${1:-${SCRIPT_DIR}/../src/stockfish}"
VARIANTS="${2:-${SCRIPT_DIR}/../src/variants.ini}"

run_cmds() {
  local cmds="$1"
  printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value sacrifice\n%s\nquit\n' \
    "$VARIANTS" "$cmds" | "$ENGINE"
}

echo "sacrifice regression started"

out=$(run_cmds "position startpos
go perft 1")
echo "$out" | grep -q "^h2h2x: 1$"
echo "$out" | grep -q "^a2a2x: 1$"
! echo "$out" | grep -q "^g1g1x: 1$"

echo "sacrifice regression passed"
