#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ENGINE="${1:-${SCRIPT_DIR}/../src/stockfish}"
VARIANTS="${2:-${SCRIPT_DIR}/../src/variants.ini}"

run_cmds() {
  local cmds="$1"
  printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value hindustani\n%s\nquit\n' \
    "$VARIANTS" "$cmds" | "$ENGINE" 2>/dev/null
}

echo "hindustani regression started"

out=$(run_cmds "position startpos
go perft 1")
echo "$out" | grep -q "^e1d3: 1$"
echo "$out" | grep -q "^e1f3: 1$"

out=$(run_cmds "position startpos moves e1d3
go perft 1")
! echo "$out" | grep -q "^d3b2: 1$"
! echo "$out" | grep -q "^d3f2: 1$"

out=$(run_cmds "position fen 3k4/8/8/8/8/8/r7/4K3 b E - 0 1 moves a2e2
go perft 1")
! echo "$out" | grep -q "^e1d3: 1$"
! echo "$out" | grep -q "^e1f3: 1$"

echo "hindustani regression passed"
