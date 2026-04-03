#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
ENGINE="${1:-${REPO_ROOT}/src/stockfish}"
VARIANTS=${2:-src/${REPO_ROOT}/src/variants.ini}

run_cmds() {
  printf 'uci\nsetoption name VariantPath value %s\n%s\nquit\n' "$VARIANTS" "$1" | "$ENGINE"
}

echo "manzhouqi regression tests started"

out=$(run_cmds "setoption name UCI_Variant value manzhouqi
position startpos
d")
echo "$out" | grep -Fq "Fen: rnbakabnr/9/1c5c1/p1p1p1p1p/9/9/P1P1P1P1P/9/9/M1BAKAB2 w - - 0 1"

out=$(run_cmds "setoption name UCI_Variant value manzhouqi
position fen 3k5/9/9/9/9/9/9/9/9/M3K4 w - - 0 1
go perft 1")
echo "$out" | grep -Fq "a1a5: 1"
echo "$out" | grep -Fq "a1b3: 1"

out=$(run_cmds "setoption name UCI_Variant value manzhouqi
position fen 3k5/9/9/9/9/9/p8/9/P8/M3K4 w - - 0 1
go perft 1")
echo "$out" | grep -Fq "a1a4: 1"

out=$(run_cmds "setoption name UCI_Variant value manzhouqi
position fen 3k5/9/9/9/9/9/9/9/P8/M3K4 w - - 0 1
go perft 1")
! echo "$out" | grep -Fq "a1a4: 1"

echo "manzhouqi regression tests passed"