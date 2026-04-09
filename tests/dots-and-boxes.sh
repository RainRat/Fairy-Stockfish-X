#!/usr/bin/env bash
set -euo pipefail

ENGINE="${1:-/home/chris/Fairy-Stockfish-X/src/stockfish}"
VARIANTS_MAIN="${2:-/home/chris/Fairy-Stockfish-X/src/variants.ini}"
VARIANTS_INCOMPLETE="${3:-/home/chris/Fairy-Stockfish-X/src/variants-incomplete.ini}"
ENGINE_LARGE="${4:-/home/chris/Fairy-Stockfish-X/src/stockfish-large}"

run_cmds() {
  local variants="$1"
  local variant="$2"
  local cmds="$3"
  printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value %s\n%s\nquit\n' \
    "$variants" "$variant" "$cmds" | "$ENGINE"
}

echo "dots and boxes regression started"

out=$(run_cmds "$VARIANTS_INCOMPLETE" dots-boxes-2x2 \
  "position startpos
go perft 1")
echo "$out" | grep -q "Nodes searched: 12"

out=$(run_cmds "$VARIANTS_MAIN" dots-boxes-7x7 \
  "position startpos
go perft 1")
echo "$out" | grep -q "Nodes searched: 24"

if [[ -x "${ENGINE_LARGE}" ]]; then
  out=$(printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value %s\nposition startpos\ngo perft 1\nquit\n' \
    "$VARIANTS_MAIN" "dots-boxes-9x9" | "$ENGINE_LARGE")
  echo "$out" | grep -q "info string variant dots-boxes-9x9 "
  echo "$out" | grep -q "Nodes searched: 40"
fi

out=$(run_cmds "$VARIANTS_INCOMPLETE" dots-boxes-2x2 \
  "position startpos moves a1a1,b5 a1a1,a4 a1a1,b3 a1a1,c4
d
go perft 1")
echo "$out" | grep -Fq "Fen: ***1*/*b*2/***1*/5/*1*1* b - - 4 3"
echo "$out" | grep -q "0000: 1"
echo "$out" | grep -q "Nodes searched: 1"

out=$(run_cmds "$VARIANTS_INCOMPLETE" dots-boxes-2x2 \
  "position startpos moves a1a1,b5 a1a1,a4 a1a1,b3 a1a1,c4 0000
d
go perft 1")
echo "$out" | grep -Fq "Fen: ***1*/*b*2/***1*/5/*1*1* b - - 5 3"
echo "$out" | grep -q "Nodes searched: 8"

python3 - <<'PY'
import pyffish as sf
from pathlib import Path

sf.load_variant_config(Path("/home/chris/Fairy-Stockfish-X/src/variants.ini").read_text())
sf.load_variant_config(Path("/home/chris/Fairy-Stockfish-X/src/variants-incomplete.ini").read_text())

assert sf.game_result("dots-boxes-2x2", "*****/*B*B*/*****/*B*B*/***** w - - 12 7", []) > 0
assert sf.game_result("dots-boxes-2x2", "*****/*B*B*/*****/*B*b*/***** w - - 12 7", []) > 0
assert sf.game_result("dots-boxes-2x2", "*****/*B*B*/*****/*b*b*/***** w - - 12 7", []) == sf.VALUE_DRAW
assert sf.game_result("dots-boxes-2x2", "*****/*B*b*/*****/*b*b*/***** w - - 12 7", []) < 0
assert sf.legal_moves("dots-boxes-2x2", "*****/*B*B*/*****/*B*B*/***** w - - 12 7", []) == []
assert sorted(sf.legal_moves("dots-boxes-2x2", "***1*/*b*2/***1*/5/*1*1* b - - 4 3", [])) == sorted([
    "a1a1,b1", "a1a1,d1", "a1a1,a2", "a1a1,c2", "a1a1,e2", "a1a1,d3", "a1a1,e4", "a1a1,d5"
])
PY

echo "dots and boxes regression passed"
