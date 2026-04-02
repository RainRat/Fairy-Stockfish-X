#!/usr/bin/env bash
set -euo pipefail

ENGINE="${1:-/home/chris/Fairy-Stockfish-X/src/stockfish}"
VARIANTS="${2:-/home/chris/Fairy-Stockfish-X/src/variants-incomplete.ini}"

run_cmds() {
  local variant="$1"
  local cmds="$2"
  printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value %s\n%s\nquit\n' \
    "$VARIANTS" "$variant" "$cmds" | "$ENGINE"
}

echo "dots and boxes prototype started"

out=$(run_cmds dots-boxes-2x2 \
  "position startpos
go perft 1")
echo "$out" | grep -q "Nodes searched: 12"

out=$(run_cmds dots-boxes-2x2 \
  "position startpos moves a1a1,b5 a1a1,a4 a1a1,b3 a1a1,c4
d
go perft 1")
echo "$out" | grep -Fq "Fen: ***1*/b2/***1*/5/*1*1* b - - 4 2"
echo "$out" | grep -q "Nodes searched: 8"

echo "dots and boxes prototype passed"
