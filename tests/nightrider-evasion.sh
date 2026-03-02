#!/bin/bash
# verify piece-aware between_bb check evasion handling for nightriders

set -euo pipefail

run() {
  local fen="$1"
  local searchmove="$2"
  printf "uci\nsetoption name VariantPath value variants.ini\nsetoption name UCI_Variant value nightrider\nposition fen %s\ngo depth 1 searchmoves %s\nquit\n" "$fen" "$searchmove" \
    | ./stockfish
}

# Nightrider check from a1 to e3 must allow interposition on c2.
out="$(run "8/8/8/8/8/4K3/8/n1R5 w - - 0 1" "c1c2")"
echo "$out" | grep -Fq "bestmove c1c2"

# Capturing the checking nightrider is also a legal evasion.
out="$(run "8/8/8/8/8/4K3/8/n1R5 w - - 0 1" "c1a1")"
echo "$out" | grep -Fq "bestmove c1a1"

# Knight checks are non-blockable; interposition must remain illegal.
out="$(run "8/8/8/8/8/4K3/6n1/2R5 w - - 0 1" "c1c2")"
echo "$out" | grep -Fq "bestmove (none)"

echo "nightrider evasion testing OK"
