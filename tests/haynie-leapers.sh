#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
ENGINE="${1:-${REPO_ROOT}/src/stockfish}"
VARIANTS=${2:-src/${REPO_ROOT}/src/variants.ini}

run_cmds() {
  printf 'uci\nsetoption name VariantPath value %s\n%s\nquit\n' "$VARIANTS" "$1" | "$ENGINE"
}

echo "haynie leapers regression tests started"

out=$(run_cmds "setoption name UCI_Variant value haynie-leapers
position startpos
go perft 1")
echo "$out" | grep -q "Nodes searched: 28"
echo "$out" | grep -q "^a1c4: 1$"
echo "$out" | grep -q "^c1b3: 1$"
echo "$out" | grep -q "^b1a4: 1$"

out=$(run_cmds "setoption name UCI_Variant value haynie-leapers
position fen k7/7P/8/8/8/8/8/7K w - - 0 1
go perft 1")
echo "$out" | grep -q "^h7h8z: 1$"
echo "$out" | grep -q "^h7h8c: 1$"
echo "$out" | grep -q "^h7h8w: 1$"
! echo "$out" | grep -q "^h7h8: 1$"

echo "haynie leapers regression tests passed"