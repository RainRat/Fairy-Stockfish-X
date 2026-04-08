#!/usr/bin/env bash
set -euo pipefail

ENGINE="${1:-/home/chris/Fairy-Stockfish-X/src/stockfish}"
VARIANTS="${2:-/home/chris/Fairy-Stockfish-X/src/variants.ini}"

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

out=$(run_cmds "position startpos moves h2h2x
d")
echo "$out" | grep -Fq "Fen: rnbqkbnr/pppppppp/8/8/8/8/PPPPPPP1/RNBQKBNR b KQkq - 0 1"

out=$(run_cmds "position fen 4k3/8/8/8/8/8/4P3/4K3 w - - 0 1 moves e2e2x
d")
echo "$out" | grep -Fq "Fen: 4k3/8/8/8/8/8/8/4K3 b - - 0 1"

echo "sacrifice regression passed"
