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
echo "$out" | grep -Fq "Fen: ***1*/b2/***1*/5/*1*1* w - - 4 3"
echo "$out" | grep -q "0000: 1"
echo "$out" | grep -q "Nodes searched: 1"

out=$(run_cmds dots-boxes-2x2 \
  "position startpos moves a1a1,b5 a1a1,a4 a1a1,b3 a1a1,c4 0000
d
go perft 1")
echo "$out" | grep -Fq "Fen: ***1*/b2/***1*/5/*1*1* b - - 5 3"
echo "$out" | grep -q "Nodes searched: 8"

python3 - <<'PY'
import pyffish as sf
from pathlib import Path

sf.load_variant_config(Path("/home/chris/Fairy-Stockfish-X/src/variants-incomplete.ini").read_text())

assert sf.game_result("dots-boxes-2x2", "*****/*B*B*/*****/*B*B*/***** w - - 12 7", []) > 0
assert sf.game_result("dots-boxes-2x2", "*****/*B*B*/*****/*B*b*/***** w - - 12 7", []) > 0
assert sf.game_result("dots-boxes-2x2", "*****/*B*B*/*****/*b*b*/***** w - - 12 7", []) == sf.VALUE_DRAW
assert sf.game_result("dots-boxes-2x2", "*****/*B*b*/*****/*b*b*/***** w - - 12 7", []) < 0
assert sf.legal_moves("dots-boxes-2x2", "*****/*B*B*/*****/*B*B*/***** w - - 12 7", []) == []
PY

echo "dots and boxes prototype passed"
