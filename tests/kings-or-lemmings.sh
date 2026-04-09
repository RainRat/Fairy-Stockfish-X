#!/usr/bin/env bash
set -euo pipefail

ENGINE="${1:-/home/chris/Fairy-Stockfish-X/src/stockfish}"
VARIANTS="${2:-/home/chris/Fairy-Stockfish-X/src/variants.ini}"

run_cmds() {
  local cmds="$1"
  printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value kings-or-lemmings\n%s\nquit\n' \
    "$VARIANTS" "$cmds" | "$ENGINE"
}

echo "kings or lemmings regression started"

out=$(run_cmds "position fen 4k3/8/8/8/8/8/8/4K3 w - - 0 1
go perft 1")
echo "$out" | grep -q "^e1e2: 1$"
echo "$out" | grep -q "^e1e2c: 1$"

out=$(run_cmds "position fen 4k3/8/8/8/8/8/4p3/4K3 w - - 0 1 moves e1e2c
d")
echo "$out" | grep -Fq "Fen: 4k3/8/8/8/8/8/4K3/4K3 b - - 0 1"

out=$(run_cmds "position fen 8/8/8/8/8/8/8/R3K2R w KQ - 0 1
go perft 1")
echo "$out" | grep -q "^e1g1: 1$"
echo "$out" | grep -q "^e1c1: 1$"

out=$(run_cmds "position fen 8/8/8/8/8/8/8/R3K2R w KQ - 0 1 moves e1e2c
d")
echo "$out" | grep -Fq "Fen: 8/8/8/8/8/8/4K3/R3K2R b - - 1 1"

out=$(run_cmds "position fen 7k/8/8/8/8/8/rq6/K6K w - - 0 1
go depth 1")
echo "$out" | grep -q "^bestmove (none)$"

echo "kings or lemmings regression passed"
