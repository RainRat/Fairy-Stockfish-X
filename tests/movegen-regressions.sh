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
out=$(run_cmds "$ROOT_DIR/src/variants.ini" chess \
  "position fen 7k/4P3/8/8/8/8/8/4K3 w - - 0 1
go perft 1")
echo "$out" | grep -q "e7e8q: 1"

# Built-in Berolina should not regain orthodox forward pawn pushes.
out=$(printf 'uci\nsetoption name UCI_Variant value berolina\nposition startpos\ngo perft 1\nquit\n' | "$ENGINE")
echo "$out" | grep -q "a2b3: 1"
echo "$out" | grep -q "a2c4: 1"
! echo "$out" | grep -q "^a2a3:"
! echo "$out" | grep -q "^a2a4:"

# Direct king capture must end the game immediately in capture-the-royal flows.
out=$(run_cmds "$ROOT_DIR/src/variants.ini" british-chess \
  "position fen 10/10/10/10/10/10/10/10/4q5/3Q6 w - - 0 1 moves d1e2
go perft 1")
echo "$out" | grep -q "Nodes searched: 0"

out=$(run_cmds "$ROOT_DIR/src/variants.ini" minihexchess \
  "position fen ***4/**5/*k5/7/6*/5**/KR2*** w - - 0 1 moves b1b5
go perft 1")
echo "$out" | grep -q "Nodes searched: 0"

# wallOrMove should not crash when the side to move has no pieces.
out=$(run_cmds "$TMP_INI" wallpass \
  "position startpos
go perft 1")
echo "$out" | grep -q "Nodes searched:"

# Racing Kings must not grant generic pawn-style initial pushes to non-pawns.
out=$(run_cmds "$ROOT_DIR/src/variants.ini" racingkings \
  "position startpos
go perft 1")
echo "$out" | grep -q "Nodes searched: 21"
! echo "$out" | grep -q "^h2h4:"
! echo "$out" | grep -q "^e2e3:"
! echo "$out" | grep -q "^e2e4:"
! echo "$out" | grep -q "^f2f3:"
! echo "$out" | grep -q "^f2f4:"

# Kings Valley pieces use the maximum-distance rule, not ordinary queen slides.
out=$(run_cmds "$ROOT_DIR/src/variants.ini" kings-valley \
  "position startpos
go perft 1")
echo "$out" | grep -q "Nodes searched: 13"
echo "$out" | grep -q "^a1d4: 1$"
echo "$out" | grep -q "^b1e4: 1$"
echo "$out" | grep -q "^c1a3: 1$"
echo "$out" | grep -q "^c1c4: 1$"
echo "$out" | grep -q "^c1e3: 1$"
! echo "$out" | grep -q "^a1a2:"
! echo "$out" | grep -q "^a1b2:"
! echo "$out" | grep -q "^c1c2:"
! echo "$out" | grep -q "^d1d2:"

echo "movegen regressions passed"
