#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
ENGINE="${1:-${REPO_ROOT}/src/stockfish}"
VARIANTS="${2:-${REPO_ROOT}/src/variants-incomplete.ini}"

run_cmds() {
  local cmds="$1"
  printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value battleotk\n%s\nquit\n' \
    "$VARIANTS" "$cmds" | "$ENGINE"
}

echo "battleotk prototype started"

out=$(run_cmds "position startpos
go perft 1")
echo "$out" | grep -q "^e2e4n: 1$"
! echo "$out" | grep -q "^e2e4: 1$"

out=$(run_cmds "position startpos moves e2e4n
d")
echo "$out" | grep -Fq "Fen: 8/pppppppp/8/8/4P3/8/PPPPNPPP/8 b - - 0 1"

out=$(run_cmds "position fen 8/6P1/8/8/8/8/8/8 w - - 0 1 moves g7g8n
d")
echo "$out" | grep -Fq "Fen: 6N1/6N1/8/8/8/8/8/8 b - - 0 1"

out=$(run_cmds "position startpos
go depth 1")
echo "$out" | grep -q "^bestmove "
! echo "$out" | grep -q "^bestmove (none)$"

out=$(run_cmds "position fen 8/8/8/8/8/8/1k6/K7 w - - 0 1 moves a1b2
go depth 1")
echo "$out" | grep -q "^bestmove (none)$"

out=$(run_cmds "position fen 8/8/8/8/8/8/1k6/K7 w - - 0 1
go depth 2")
echo "$out" | grep -q "^bestmove a1b2$"

out=$(run_cmds "position fen 8/8/8/8/8/8/1k6/K7 w - - 0 1
go depth 2 searchmoves a1b1 a1a2 a1b2")
echo "$out" | grep -q "^bestmove a1b2$"

out=$(run_cmds "position fen 8/8/8/8/8/8/1kk5/K7 w - - 0 1 moves a1b2
go depth 1")
echo "$out" | grep -q "^bestmove "
! echo "$out" | grep -q "^bestmove (none)$"

echo "battleotk prototype passed"
