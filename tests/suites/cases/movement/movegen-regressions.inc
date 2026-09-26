#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/../../../../tests/lib/uci.sh"

ENGINE=$(default_engine "${1:-}")
VARIANTS=$(default_variants "${2:-}")

TMP_INI="$(mktemp)"
HOOK_INI="$(mktemp)"
cleanup() {
  rm -f "$TMP_INI" "$HOOK_INI"
}
trap cleanup EXIT

cat >"$HOOK_INI" <<'EOF'
[test-hook]
king = -
customPiece1 = h:R
hookMoves = h:R-R
startFen = 8/8/8/3h4/4H3/8/8/8 w - - 0 1
immobile = -
castling = false

[test-hook-bent]
king = -
customPiece1 = h:0
hookMoves = h:R-R
startFen = 8/8/8/5h2/4H3/8/8/8 w - - 0 1
immobile = -
castling = false

[test-hook-capture]
king = -
customPiece1 = h:0
hookMoves = h:R-R
startFen = 8/8/8/8/3hH3/8/8/8 w - - 0 1
immobile = -
castling = false

[test-hook-first-capture-stops]
king = -
customPiece1 = h:R
hookMoves = h:R-R
startFen = 8/8/8/3H4/3hH3/8/8/8 w - - 0 1
immobile = -
castling = false

[test-hook-overlap]
king = -
customPiece1 = h:R
hookMoves = h:R-R
startFen = 8/8/8/8/3hH3/8/8/8 w - - 0 1
immobile = -
castling = false

[test-hook-bend-mobility:chess]
customPiece1 = h:0
hookMoves = h:R-R
mobilityRegionWhiteCustomPiece1 = d5
startFen = 8/8/8/3k4/2H5/8/8/7K b - - 0 1
castling = false

[test-royal-route:chess]
king = k:R
twoStepMoves = k:NW>NE
royalPieceNoThroughCheck = true
startFen = k7/8/3b4/8/4K3/8/8/8 w - - 0 1
castling = false
EOF

cat >"$TMP_INI" <<'EOF'
[wallpass:chess]
wallingRule = edge
wallOrMove = true
flagRegionWhite = *
flagRegionBlack = *
startFen = 4k3/8/8/8/8/8/8/8 w - - 0 1

[edge-wall-anchor:fairy]
king = -
checking = false
wallingRule = edge
wallOrMove = true
flagRegionWhite = *8
flagRegionBlack = *1
startFen = 8/8/8/8/8/8/8/8 w - - 0 1

[duck-wall-anchor:fairy]
king = -
checking = false
wallingRule = duck
wallingWhite = true
wallingBlack = true
wallOrMove = true
flagRegionWhite = *8
flagRegionBlack = *1
startFen = 8/8/8/8/8/8/8/8 w - - 0 1

[wall-split:fairy]
king = -
checking = false
wallingRule = edge
wallOrMove = true
wallingRegionWhite = a1
wallingRegionBlack = h8
startFen = 8/8/8/8/8/8/8/8 w - - 0 1

[wall-color-split:fairy]
king = -
checking = false
wallingRule = edge
wallingWhite = false
wallingBlack = true
wallOrMove = true
wallingRegionWhite = a1
wallingRegionBlack = h8
startFen = 8/8/8/8/8/8/8/8 w - - 0 1

[wall-or-move-disabled:chess]
wallOrMove = true
startFen = 4k3/8/8/8/8/8/8/4K3 w - - 0 1

[wazir-chess:chess]
king = w:W
startFen = W6w/QQ6/8/8/8/8/8/8 w

# Promotion-decline boundaries: leaving-zone captures re-offer promotion;
# pawns get a last-rank non-capture retry.
[test-decline-leave-capture:chess]
customPiece1 = a:R
promotedPieceType = a:q
promotionRegionWhite = *7 *8
promotionRegionBlack = *2 *1
promotionDeclineRule = true
mandatoryPawnPromotion = false
mandatoryPiecePromotion = false
castling = false
doubleStep = false

[test-decline-pawn-last-rank:chess]
promotionRegionWhite = *7 *8
promotionRegionBlack = *2 *1
promotionDeclineRule = true
mandatoryPawnPromotion = false
mandatoryPiecePromotion = false
castling = false
doubleStep = false

[pawn-explicit-initial:fairy]
customPiece1 = p:iW
pawnTypes = p
startFen = 4k3/8/8/8/8/8/4P3/4K3 w - - 0 1

# Lion-family and hook-mover regression fixtures. These live here (not in
# src/variants.ini) so the production variant list stays free of test-only
# entries; run_uci_cmds addresses them through this file.

# Lion: two king steps in any direction (*), direct jumps (ADNW), step 1 (K).
[test-lion]
king = -
customPiece1 = l:ADNWK
twoStepMoves = l:*
startFen = 8/8/8/3l4/4L3/8/8/8 w - - 0 1
immobile = -
castling = false
doubleStep = false

[test-lion-blocked-jump:test-lion]
# Both knight-route vias (e5, f5) are friendly L: proves the direct N jump
# is independent of the two-step routes, which must be suppressed here.
startFen = 8/8/8/4LL2/4L3/8/8/8 w - - 0 1

# Horned Falcon: Bishop (B), sideways/backward slides (sRbR), forward jump 2
# (fD), forward step 1 (fW), forward two-step (N>N,N>S).
[test-horned-falcon]
king = -
customPiece1 = h:BsRbRfDfW
twoStepMoves = h:N>N,N>S
startFen = 8/8/8/3h4/4H3/8/8/8 w - - 0 1
immobile = -
castling = false
doubleStep = false

# Soaring Eagle: Rook (R), backward-diagonal slides (bB), forward diagonal
# jump 2 (fA) and step 1 (fF), forward-diagonal two-step.
[test-soaring-eagle]
king = -
customPiece1 = e:RbBfAfF
twoStepMoves = e:NE>NE,NE>SW,NW>NW,NW>SE
startFen = 8/8/8/3e4/4E3/8/8/8 w - - 0 1
immobile = -
castling = false
doubleStep = false

# Free Eagle: Queen (Q), diagonal jump 2 (A), diagonal cat-sword two-step.
[test-free-eagle]
king = -
customPiece1 = f:QA
twoStepMoves = f:NE>NE,NE>NW,NE>SE,NE>SW,NW>NE,NW>NW,NW>SE,NW>SW,SE>NE,SE>NW,SE>SE,SE>SW,SW>NE,SW>NW,SW>SE,SW>SW
startFen = 8/8/8/3f4/4F3/8/8/8 w - - 0 1
immobile = -
castling = false
doubleStep = false

# Lion Hawk: Bishop (B) with full Lion jumps (ADNWK) and two-step (*).
[test-lion-hawk]
king = -
customPiece1 = y:BADNWK
twoStepMoves = y:*
startFen = 8/8/8/3y4/4Y3/8/8/8 w - - 0 1
immobile = -
castling = false
doubleStep = false

# Hook mover (single capture): rook movement plus an optional turn, at most one capture.
[test-hook]
king = -
customPiece1 = h:R
hookMoves = h:R-R
startFen = 8/8/8/3h4/4H3/8/8/8 w - - 0 1
immobile = -
castling = false
doubleStep = false

# Hook mover bent-path only: straight captures belong to the Betza rider.
[test-hook-bent]
king = -
customPiece1 = h:0
hookMoves = h:R-R
startFen = 8/8/8/5h2/4H3/8/8/8 w - - 0 1
immobile = -
castling = false
doubleStep = false
EOF

variant_available() {
  local variant_path="$1"
  local variant="$2"

  probe_variant_available "$ENGINE" "$variant" "$variant_path"
}

echo "movegen regressions started"

# Quiet pawn promotions must be generated in quiet move generation.
out=$(run_uci_cmds "$ENGINE" "$VARIANTS" chess \
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
out=$(run_uci_cmds "$ENGINE" "$VARIANTS" british-chess \
  "position fen 10/10/10/10/10/10/10/10/4q5/3Q6 w - - 0 1 moves d1e2
go perft 1")
assert_nodes "$out" "0"

if variant_available "$VARIANTS" minihexchess; then
  out=$(run_uci_cmds "$ENGINE" "$VARIANTS" minihexchess \
    "position fen ***4/**5/*k5/7/6*/5**/KR2*** w - - 0 1 moves b1b5
go perft 1")
  assert_nodes "$out" "0"
fi

# Custom king movement must still participate in orthodox checkmate semantics.
out=$(run_uci_cmds "$ENGINE" "$TMP_INI" wazir-chess \
  "position startpos moves b7h7 h8h7 a7h7
go perft 1")
assert_nodes "$out" "0"

# Tablut-family surround capture of the king must also end immediately.
out=$(run_uci_cmds "$ENGINE" "$VARIANTS" brandub \
  "position fen 4r2/7/3r3/2rK3/3r3/7/7 b - - 0 1 moves e7e4
go perft 1")
assert_nodes "$out" "0"

# Anti extinction variants using "*" must not end when a single piece class is gone.
out=$(run_uci_cmds "$ENGINE" "$VARIANTS" antiminishogi \
  "position startpos
go perft 1")
assert_nodes "$out" "1"
assert_contains "$out" "^e1e4: 1$"

out=$(run_uci_cmds "$ENGINE" "$VARIANTS" anti-losalamos \
  "position fen rn1knr/pppppp/6/6/PPPPPP/RNQKNR w - - 0 1
go perft 1")
assert_nodes "$out" "10"

out=$(run_uci_cmds "$ENGINE" "$VARIANTS" chaturanga-al-adli \
  "position fen brn1knrb/pppppppp/8/8/8/8/PPPPPPPP/BRNFKNRB w - - 0 1
go perft 1")
assert_nodes "$out" "14"

# wallOrMove should not crash when the side to move has no pieces.
out=$(run_uci_cmds "$ENGINE" "$TMP_INI" wallpass \
  "position startpos
go perft 1")
assert_contains "$out" "Nodes searched:"

# Pure wall-or-move generation must include the encoding anchor itself when it
# is a legal empty wall target.
out=$(run_uci_cmds "$ENGINE" "$TMP_INI" edge-wall-anchor \
  "position startpos
go perft 1")
assert_contains "$out" "^0000,a1: 1$"

# Duck wall-or-move uses the same gating emission path but keeps unrestricted
# wall placement instead of edge-adjacency filtering.
out=$(run_uci_cmds "$ENGINE" "$TMP_INI" duck-wall-anchor \
  "position startpos
go perft 1")
assert_contains "$out" "^0000,d4: 1$"

out=$(run_uci_cmds "$ENGINE" "$TMP_INI" wall-split \
  "position startpos
go perft 1")
assert_contains "$out" "^0000,a1: 1$"
assert_not_contains "$out" "^0000,h8: 1$"

out=$(run_uci_cmds "$ENGINE" "$TMP_INI" wall-split \
  "position fen 8/8/8/8/8/8/8/8 b - - 0 1
go perft 1")
assert_contains "$out" "^0000,h8: 1$"

out=$(run_uci_cmds "$ENGINE" "$TMP_INI" wall-color-split \
  "position startpos
go perft 1")
assert_not_contains "$out" "^0000,a1: 1$"

out=$(run_uci_cmds "$ENGINE" "$TMP_INI" wall-color-split \
  "position fen 8/8/8/8/8/8/8/8 b - - 0 1
go perft 1")
assert_contains "$out" "^0000,h8: 1$"

# wallOrMove alone should not emit pseudo-special wall anchors when no walling
# rule is enabled for the side to move.
out=$(run_uci_cmds "$ENGINE" "$TMP_INI" wall-or-move-disabled \
  "position startpos
go perft 1")
assert_nodes "$out" "5"

# Duck wall relocation uses gating encoding without a gated piece.
out=$(run_uci_cmds "$ENGINE" "$VARIANTS" atomicduck \
  "position startpos moves a2a3,a3a2
go depth 2")
assert_contains "$out" "^bestmove "

# Racing Kings must not grant generic pawn-style initial pushes to non-pawns.
out=$(run_uci_cmds "$ENGINE" "$VARIANTS" racingkings \
  "position startpos
go perft 1")
assert_nodes "$out" "21"
assert_not_contains "$out" "^h2h4:"
assert_not_contains "$out" "^e2e3:"
assert_not_contains "$out" "^e2e4:"
assert_not_contains "$out" "^f2f3:"
assert_not_contains "$out" "^f2f4:"

# A pawn with explicit initial W moves must use the generic move generator.
out=$(run_uci_cmds "$ENGINE" "$TMP_INI" pawn-explicit-initial \
  "position fen 4k3/8/8/8/8/8/4P3/4K3 w - - 0 1
go perft 1")
assert_contains "$out" "^e2d2: 1$"
assert_contains "$out" "^e2f2: 1$"
assert_contains "$out" "^e2e3: 1$"
assert_not_contains "$out" "^e2e4:"

# Kings Valley pieces use the maximum-distance rule, not ordinary queen slides.
out=$(run_uci_cmds "$ENGINE" "$VARIANTS" kings-valley \
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

# Reaching the valley ends King's Valley immediately; do not grant a reply move.
out=$(run_uci_cmds "$ENGINE" "$VARIANTS" kings-valley \
  "position fen 1s1ss/2KS1/2ks1/S4/1S1S1 w - - 8 5
go depth 1")
assert_contains_literal "$out" "bestmove (none)"

# Oshi search should not prefer handing the opponent a point by self-ejecting.
out=$(run_uci_cmds "$ENGINE" "$VARIANTS" oshi \
  "position fen ca2a4/b4ab1c/4a4/9/5A3/2AC5/9/2BAA1B2/C8 w - - 10 6 {0 0}
go depth 8")
assert_not_contains "$out" "^bestmove d4a4"

# Lion family two-step move regressions: unique outcomes, igui, quiet pass, and gating.
out=$(run_uci_cmds "$ENGINE" "$TMP_INI" test-lion \
  "position startpos
go perft 1")
assert_nodes "$out" "33"
assert_contains "$out" "^e4d5e4: 1$"
assert_contains "$out" "^e4e5e4: 1$"
assert_contains "$out" "^e4e6: 1$"
assert_contains "$out" "^e4d5c6: 1$"
assert_not_contains "$out" "^e4f4f5:"
assert_not_contains "$out" "^e4e5f5:"

out=$(run_uci_cmds "$ENGINE" "$TMP_INI" test-lion-blocked-jump \
  "position startpos
go perft 1")
assert_contains "$out" "^e4f6: 1$"
# Both knight-route vias (e5, f5) are friendly-occupied, so the two-step
# spellings are blocked while the direct N jump still reaches f6.
assert_not_contains "$out" "^e4e5f6:"
assert_not_contains "$out" "^e4f5f6:"

out=$(run_uci_cmds "$ENGINE" "$TMP_INI" test-lion \
  "position startpos moves e4d5e4
d")
assert_contains_literal "$out" "Fen: 8/8/8/8/4L3/8/8/8 b - - 0 1"

out=$(run_uci_cmds "$ENGINE" "$TMP_INI" test-lion \
  "position startpos moves e4e5e4
d")
assert_contains_literal "$out" "Fen: 8/8/8/3l4/4L3/8/8/8 b - - 1 1"

out=$(run_uci_cmds "$ENGINE" "$TMP_INI" test-horned-falcon \
  "position startpos
go perft 1")
assert_nodes "$out" "23"
assert_contains "$out" "^e4e6: 1$"
assert_contains "$out" "^e4e5e4: 1$"
assert_not_contains "$out" "^e4e3e2:"
assert_not_contains "$out" "^e4e3e4:"

out=$(run_uci_cmds "$ENGINE" "$TMP_INI" test-soaring-eagle \
  "position startpos
go perft 1")
assert_nodes "$out" "27"
assert_contains "$out" "^e4d5e4: 1$"
assert_contains "$out" "^e4d5c6: 1$"
assert_contains "$out" "^e4g6: 1$"
assert_not_contains "$out" "^e4e5e6:"

out=$(run_uci_cmds "$ENGINE" "$TMP_INI" test-free-eagle \
  "position startpos
go perft 1")
assert_nodes "$out" "30"
assert_contains "$out" "^e4d5e4: 1$"
assert_contains "$out" "^e4d5c6: 1$"
assert_contains "$out" "^e4g6: 1$"
assert_contains "$out" "^e4f5e4: 1$"
assert_not_contains "$out" "^e4e5e6:"
assert_not_contains "$out" "^e4e5e4:"

out=$(run_uci_cmds "$ENGINE" "$TMP_INI" test-lion-hawk \
  "position startpos
go perft 1")
assert_nodes "$out" "36"
assert_contains "$out" "^e4d5e4: 1$"
assert_contains "$out" "^e4e6: 1$"
assert_contains "$out" "^e4e5e4: 1$"

# Hook movers turn at right angles when both legs use the same geometry.
out=$(run_uci_cmds "$ENGINE" "$HOOK_INI" test-hook \
  "position startpos
go perft 1")
assert_contains "$out" "^e4e5d5: 1$"
assert_contains "$out" "^e4e8f8: 1$"
assert_contains "$out" "^e4e8: 1$"
assert_not_contains "$out" "^e4d5e4:"
assert_not_contains "$out" "^e4d4c4:"
assert_not_contains "$out" "^e4d4e4:"
assert_not_contains "$out" "^e4d4d8:"

# Bent-path only: with no Betza movement, a hook gets no straight capture
# (e4d4d4 absent) and cannot turn after a first-leg capture (e4d4d5 absent).
out=$(run_uci_cmds "$ENGINE" "$HOOK_INI" test-hook-capture \
  "position startpos
go perft 1")
assert_not_contains "$out" "^e4d4d4:"
assert_not_contains "$out" "^e4d4d5:"

out=$(run_uci_cmds "$ENGINE" "$HOOK_INI" test-hook \
  "position startpos moves e4e5d5
d")
assert_contains_literal "$out" "Fen: 8/8/8/3H4/8/8/8/8 b - - 0 1"

out=$(run_uci_cmds "$ENGINE" "$HOOK_INI" test-hook-bent \
  "position startpos
go perft 1")
assert_contains "$out" "^e4e5f5: 1$"
assert_not_contains "$out" "^e4e5e6:"

# A hook cannot capture on the first leg and then turn: it must stop.
# Straight captures still work through the ordinary Betza rider.
out=$(run_uci_cmds "$ENGINE" "$HOOK_INI" test-hook-first-capture-stops \
  "position startpos
go perft 1")
assert_contains "$out" "^e4d4: 1$"
assert_not_contains "$out" "^e4d4d4:"
assert_not_contains "$out" "^e4d4d5:"

out=$(run_uci_cmds "$ENGINE" "$HOOK_INI" test-hook-overlap \
  "position startpos
go perft 1")
assert_contains "$out" "^e4d4: 1$"
assert_not_contains "$out" "^e4d4d4:"
assert_not_contains "$out" "^e4e5e7:"

# A hook gives check with a bent capture even when the bend itself is outside
# the mover's mobility region, as long as the capture ends inside it.
out=$(run_uci_cmds "$ENGINE" "$HOOK_INI" test-hook-bend-mobility \
  "position startpos
d")
assert_contains "$out" "^Checkers: c4"

# The direct rook route e4-e6 crosses an attacked square, while the two-step
# royal route e4-d5-e6 reaches the same endpoint without crossing it.
out=$(run_uci_cmds "$ENGINE" "$HOOK_INI" test-royal-route \
  "position startpos
go perft 1")
assert_contains "$out" "^e4d5e6: 1$"

# A declined Chu promotion can be taken on a capture starting in-zone and ending outside.
out=$(run_uci_cmds "$ENGINE" "$TMP_INI" test-decline-leave-capture \
  "position fen 7k/8/1A6/1p6/8/8/8/K7 w - - 0 1 moves b6b7 h8g8
go perft 1")
assert_contains "$out" "^b7b5+: 1$"

# A declined pawn gets a last-rank non-capture retry.
out=$(run_uci_cmds "$ENGINE" "$TMP_INI" test-decline-pawn-last-rank \
  "position fen 7k/8/P7/8/8/8/8/K7 w - - 0 1 moves a6a7 h8g8
go perft 1")
assert_contains "$out" "^a7a8q: 1$"

echo "movegen regressions passed"
