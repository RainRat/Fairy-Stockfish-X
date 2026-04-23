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

[wazir-chess:chess]
king = w:W
startFen = W6w/QQ6/8/8/8/8/8/8 w
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

# Custom king movement must still participate in orthodox checkmate semantics.
out=$(run_cmds "$TMP_INI" wazir-chess \
  "position startpos moves b7h7 h8h7 a7h7
go perft 1")
echo "$out" | grep -q "Nodes searched: 0"

# Tablut-family surround capture of the king must also end immediately.
out=$(run_cmds "$ROOT_DIR/src/variants.ini" brandub \
  "position fen 4r2/7/3r3/2rK3/3r3/7/7 b - - 0 1 moves e7e4
go perft 1")
echo "$out" | grep -q "Nodes searched: 0"

# Anti extinction variants using "*" must not end when a single piece class is gone.
out=$(run_cmds "$ROOT_DIR/src/variants.ini" antiminishogi \
  "position startpos
go perft 1")
echo "$out" | grep -q "Nodes searched: 1"
echo "$out" | grep -q "^e1e4: 1$"

out=$(run_cmds "$ROOT_DIR/src/variants.ini" anti-losalamos \
  "position fen rn1knr/pppppp/6/6/PPPPPP/RNQKNR w - - 0 1
go perft 1")
echo "$out" | grep -q "Nodes searched: 10"

out=$(run_cmds "$ROOT_DIR/src/variants.ini" chaturanga-al-adli \
  "position fen brn1knrb/pppppppp/8/8/8/8/PPPPPPPP/BRNFKNRB w - - 0 1
go perft 1")
echo "$out" | grep -q "Nodes searched: 14"

# wallOrMove should not crash when the side to move has no pieces.
out=$(run_cmds "$TMP_INI" wallpass \
  "position startpos
go perft 1")
echo "$out" | grep -q "Nodes searched:"

# Duck wall relocation uses gating encoding without a gated piece.
out=$(run_cmds "$ROOT_DIR/src/variants.ini" atomicduck \
  "position startpos moves a2a3,a3a2
go depth 2")
echo "$out" | grep -q "^bestmove "

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

# Oshi search should not prefer handing the opponent a point by self-ejecting.
out=$(run_cmds "$ROOT_DIR/src/variants.ini" oshi \
  "position fen ca2a4/b4ab1c/4a4/9/5A3/2AC5/9/2BAA1B2/C8 w - - 10 6 {0 0}
go depth 8")
! echo "$out" | grep -q "^bestmove d4a4"

echo "movegen regressions passed"
