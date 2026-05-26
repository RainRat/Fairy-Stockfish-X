#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENGINE="${1:-$ROOT_DIR/src/stockfish}"
source "$ROOT_DIR/tests/lib/uci.sh"

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

[pawn-explicit-initial:fairy]
customPiece1 = p:iW
pawnTypes = p
startFen = 4k3/8/8/8/8/8/4P3/4K3 w - - 0 1

[swap-roundtrip:fairy]
maxFile = e
maxRank = 5
king = -
checking = false
pass = true
pieceToCharTable = -
customPiece1 = a:mW
adjacentSwapMoveTypes = a p
startFen = 5/5/5/1aP2/5 w - - 0 1
EOF

run_cmds() {
  run_uci "$ENGINE" "$1" "$2" <<< "$3"
}

variant_available() {
  local variant_path="$1"
  local variant="$2"
  local out
  out=$(run_cmds "$variant_path" "$variant" "d" || true)
  assert_contains "$out" "info string variant ${variant} "
}

echo "movegen regressions started"

# Quiet pawn promotions must be generated in quiet move generation.
out=$(run_cmds "$ROOT_DIR/src/variants.ini" chess \
  "position fen 7k/4P3/8/8/8/8/8/4K3 w - - 0 1
go perft 1")
assert_contains "$out" "e7e8q: 1"

# Built-in Berolina should not regain orthodox forward pawn pushes.
out=$(printf 'uci\nsetoption name UCI_Variant value berolina\nposition startpos\ngo perft 1\nquit\n' | "$ENGINE")
assert_contains "$out" "a2b3: 1"
assert_contains "$out" "a2c4: 1"
assert_not_contains "$out" "^a2a3:"
assert_not_contains "$out" "^a2a4:"

# Direct king capture must end the game immediately in capture-the-royal flows.
out=$(run_cmds "$ROOT_DIR/src/variants.ini" british-chess \
  "position fen 10/10/10/10/10/10/10/10/4q5/3Q6 w - - 0 1 moves d1e2
go perft 1")
assert_nodes "$out" "0"

if variant_available "$ROOT_DIR/src/variants.ini" minihexchess; then
  out=$(run_cmds "$ROOT_DIR/src/variants.ini" minihexchess \
    "position fen ***4/**5/*k5/7/6*/5**/KR2*** w - - 0 1 moves b1b5
go perft 1")
  assert_nodes "$out" "0"
fi

# Custom king movement must still participate in orthodox checkmate semantics.
out=$(run_cmds "$TMP_INI" wazir-chess \
  "position startpos moves b7h7 h8h7 a7h7
go perft 1")
assert_nodes "$out" "0"

# Tablut-family surround capture of the king must also end immediately.
out=$(run_cmds "$ROOT_DIR/src/variants.ini" brandub \
  "position fen 4r2/7/3r3/2rK3/3r3/7/7 b - - 0 1 moves e7e4
go perft 1")
assert_nodes "$out" "0"

# Anti extinction variants using "*" must not end when a single piece class is gone.
out=$(run_cmds "$ROOT_DIR/src/variants.ini" antiminishogi \
  "position startpos
go perft 1")
assert_nodes "$out" "1"
assert_contains "$out" "^e1e4: 1$"

out=$(run_cmds "$ROOT_DIR/src/variants.ini" anti-losalamos \
  "position fen rn1knr/pppppp/6/6/PPPPPP/RNQKNR w - - 0 1
go perft 1")
assert_nodes "$out" "10"

out=$(run_cmds "$ROOT_DIR/src/variants.ini" chaturanga-al-adli \
  "position fen brn1knrb/pppppppp/8/8/8/8/PPPPPPPP/BRNFKNRB w - - 0 1
go perft 1")
assert_nodes "$out" "14"

# wallOrMove should not crash when the side to move has no pieces.
out=$(run_cmds "$TMP_INI" wallpass \
  "position startpos
go perft 1")
assert_contains "$out" "Nodes searched:"

# Duck wall relocation uses gating encoding without a gated piece.
out=$(run_cmds "$ROOT_DIR/src/variants.ini" atomicduck \
  "position startpos moves a2a3,a3a2
go depth 2")
assert_contains "$out" "^bestmove "

# Racing Kings must not grant generic pawn-style initial pushes to non-pawns.
out=$(run_cmds "$ROOT_DIR/src/variants.ini" racingkings \
  "position startpos
go perft 1")
assert_nodes "$out" "21"
assert_not_contains "$out" "^h2h4:"
assert_not_contains "$out" "^e2e3:"
assert_not_contains "$out" "^e2e4:"
assert_not_contains "$out" "^f2f3:"
assert_not_contains "$out" "^f2f4:"

# A pawn with explicit initial W moves must use the generic move generator.
out=$(run_cmds "$TMP_INI" pawn-explicit-initial \
  "position fen 4k3/8/8/8/8/8/4P3/4K3 w - - 0 1
go perft 1")
assert_contains "$out" "^e2d2: 1$"
assert_contains "$out" "^e2f2: 1$"
assert_contains "$out" "^e2e3: 1$"
assert_not_contains "$out" "^e2e4:"

# A pawn that moved away and returned to its starting square must not regain double-step rights.
out=$(run_cmds "$TMP_INI" swap-roundtrip \
  "position fen 5/5/5/1aP2/5 w - - 0 1 moves c2b2s 0000 b2c2s 0000
go perft 1")
assert_not_contains "$out" "^c2c4:"

# Kings Valley pieces use the maximum-distance rule, not ordinary queen slides.
out=$(run_cmds "$ROOT_DIR/src/variants.ini" kings-valley \
  "position startpos
go perft 1")
assert_nodes "$out" "13"
assert_contains "$out" "^a1d4: 1$"
assert_contains "$out" "^b1e4: 1$"
assert_contains "$out" "^c1a3: 1$"
assert_contains "$out" "^c1c4: 1$"
assert_contains "$out" "^c1e3: 1$"
assert_not_contains "$out" "^a1a2:"
assert_not_contains "$out" "^a1b2:"
assert_not_contains "$out" "^c1c2:"
assert_not_contains "$out" "^d1d2:"

# Oshi search should not prefer handing the opponent a point by self-ejecting.
out=$(run_cmds "$ROOT_DIR/src/variants.ini" oshi \
  "position fen ca2a4/b4ab1c/4a4/9/5A3/2AC5/9/2BAA1B2/C8 w - - 10 6 {0 0}
go depth 8")
assert_not_contains "$out" "^bestmove d4a4"

echo "movegen regressions passed"
