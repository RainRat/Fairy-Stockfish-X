#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENGINE="${1:-$ROOT_DIR/src/stockfish-large}"

TMP_INI="$(mktemp)"
cleanup() {
  rm -f "$TMP_INI"
}
trap cleanup EXIT

cat >"$TMP_INI" <<'EOF'
[wallpass:chess]
wallingRule = edge
wallOrMove = true
flagRegionWhite = *
flagRegionBlack = *
startFen = 4k3/8/8/8/8/8/8/8 w - - 0 1
EOF

run_cmds() {
  local variant_path="$1"
  local variant="$2"
  local cmds="$3"
  printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value %s\n%s\nquit\n' \
    "$variant_path" "$variant" "$cmds" | "$ENGINE"
}

echo "movegen regressions started"

# Quiet pawn promotions must be generated in quiet move generation.
out=$(run_cmds "/home/chris/Fairy-Stockfish-X/src/variants.ini" chess \
  "position fen 7k/4P3/8/8/8/8/8/4K3 w - - 0 1
go perft 1")
echo "$out" | grep -q "e7e8q: 1"

# wallOrMove should not crash when the side to move has no pieces.
out=$(run_cmds "$TMP_INI" wallpass \
  "position startpos
go perft 1")
echo "$out" | grep -q "Nodes searched:"

echo "movegen regressions passed"
