#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")/../src"

# Basic smoke: opening move count from configured start position.
out_start=$(printf 'uci\nsetoption name VariantPath value variants.ini\nsetoption name UCI_Variant value whaleshogi\nposition startpos\ngo perft 1\nquit\n' | ./stockfish)
grep -q "Nodes searched: 7" <<<"$out_start"

# Dolphin promotion to eagle is mandatory on furthest rank.
out_promo=$(printf 'uci\nsetoption name VariantPath value variants.ini\nsetoption name UCI_Variant value whaleshogi\nposition fen 5w/4D1/6/6/6/W5 w - - 0 1\ngo perft 1\nquit\n' | ./stockfish)
grep -q "e5e6+:" <<<"$out_promo"
! grep -q "e5e6:" <<<"$out_promo"

# Promoted dolphin (+D) demotes when leaving the back rank.
out_demote=$(printf 'uci\nsetoption name VariantPath value variants.ini\nsetoption name UCI_Variant value whaleshogi\nposition fen 4+Dw/6/6/6/6/W5 w - - 0 1\ngo perft 1\nquit\n' | ./stockfish)
grep -q "e6d5-:" <<<"$out_demote"
grep -q "e6f5-:" <<<"$out_demote"
! grep -q "e6d5:" <<<"$out_demote"

echo "whaleshogi test OK"
