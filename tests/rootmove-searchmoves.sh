#!/bin/bash
# verify root move filtering stays aligned across MultiPV sorting

set -euo pipefail

engine="${1:-src/stockfish}"

run() {
  printf '%s\n' \
    "uci" \
    "setoption name MultiPV value 3" \
    "position startpos" \
    "go depth 4 searchmoves e2e4 d2d4 g1f3" \
    "quit" | "$engine"
}

run_skill() {
  printf '%s\n' \
    "uci" \
    "setoption name Skill Level value 0" \
    "position startpos" \
    "go depth 4 searchmoves e2e4 d2d4 g1f3 b1c3" \
    "quit" | "$engine"
}

out="$(run)"
best="$(printf '%s\n' "$out" | awk '/^bestmove / { print $2 }')"
[[ "$best" =~ ^(e2e4|d2d4|g1f3)$ ]]

mapfile -t pvs < <(printf '%s\n' "$out" | awk '/ multipv / { for (i = 1; i <= NF; ++i) if ($i == "pv") { print $(i + 1); break } }' | sort -u)
for mv in "${pvs[@]}"; do
  [[ "$mv" =~ ^(e2e4|d2d4|g1f3)$ ]]
done

skill_out="$(run_skill)"
skill_best="$(printf '%s\n' "$skill_out" | awk '/^bestmove / { print $2 }')"
[[ "$skill_best" =~ ^(e2e4|d2d4|g1f3|b1c3)$ ]]

echo "root move searchmoves OK"
