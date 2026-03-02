#!/bin/bash
# verify nMoveHardLimitRule semantics (full-move based)

set -euo pipefail

tmp_ini="$(mktemp)"
trap 'rm -f "$tmp_ini"' EXIT

cat > "$tmp_ini" <<'EOF'
[hardlimit-test:chess]
nMoveHardLimitRule = 200
nMoveHardLimitRuleValue = draw
EOF

run() {
  local fen="$1"
  printf "uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value hardlimit-test\nposition fen %s\ngo depth 1\nquit\n" "$tmp_ini" "$fen" \
    | ./stockfish
}

# At fullmove 200 the game is not over yet.
out="$(run "8/8/8/8/8/8/8/K6k w - - 0 200")"
echo "$out" | grep -Fq "bestmove a1b1"

# At fullmove 201 the game is immediately adjudicated.
out="$(run "8/8/8/8/8/8/8/K6k w - - 0 201")"
echo "$out" | grep -Fq "bestmove (none)"

echo "hard limit testing OK"
