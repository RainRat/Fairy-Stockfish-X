#!/bin/bash

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "unorthodox interactions test"

# Create a temporary ini file for testing
load_inline_variants <<'EOF'
[rifle-death:chess]
rifleCapture = true
deathOnCaptureTypes = nbrqk

[rifle-morph:chess]
rifleCapture = true
captureMorph = true

[rifle-atomic:chess]
blastOnCapture = true
blastOrthogonals = false
blastDiagonals = false
rifleCapture = true
castling = false
king = -
customPiece1 = k:K
extinctionValue = loss
extinctionPieceTypes = k

[rifle-color:chess]
rifleCapture = true
changingColorTrigger = capture
changingColorPieceTypes = *

[jump-blast:chess]
customPiece1 = m:c{hurdles: 1,1; pre: 1,1; post: 1,1; capture: locust_first; hurdle_types: enemy}W
blastOnSameTypeCapture = true
blastCenter = true
blastOrthogonals = false
blastDiagonals = false

[jump-blast-color:jump-blast]
changingColorTrigger = capture
changingColorPieceTypes = *

[jump-blast-mover:jump-blast]
blastOnCaptureMoverCenter = true

[rifle-jump:chess]
customPiece1 = m:c{hurdles: 1,1; pre: 1,1; post: 1,1; capture: locust_first; hurdle_types: enemy}W
rifleCapture = true

[rifle-duck:chess]
rifleCapture = true
wallingRule = duck
castling = false
king = -
customPiece1 = k:K
extinctionValue = loss
extinctionPieceTypes = k

[color-change:chess]
changingColorTrigger = always
changingColorPieceTypes = r

[rifle-forbidden:chess]
rifleCapture = true
captureForbidden = *:p

[forbidden-king-check:chess]
customPiece1 = d:Q
pieceToCharTable = PNBRQ............D...Kpnbrq............d...k
captureForbidden = d:k
startFen = 4k3/4D3/8/8/8/8/8/4K3 b - - 0 1

[forbidden-king-capture:chess]
customPiece1 = d:Q
pieceToCharTable = PNBRQ............D...Kpnbrq............d...k
captureForbidden = d:k
startFen = k7/8/8/8/8/8/8/D3K3 w - - 0 1
checking = false

[dead-fen:chess]
startFen = 4k3/8/8/8/8/8/8/8 w - - 0 1

[iron-extinction:chess]
customPiece1 = s:W
pieceToCharTable = PNBRQ............S...Kpnbrq............s...k
captureForbidden = *:s
extinctionValue = win
extinctionPieceTypes = q
checking = false
startFen = 4k3/8/8/8/8/8/3Qq3/4S2K w - - 0 1

[rifle-gating:chess]
rifleCapture = true
gating = true
seirawanGating = true

[rifle-symgating:chess]
rifleCapture = true
gating = true
seirawanGating = true
symmetricDropTypes = r

[surround-color:chess]
surroundCaptureIntervene = true
changingColorTrigger = capture
changingColorPieceTypes = *

[morph-color:chess]
captureMorph = true
changingColorTrigger = capture
changingColorPieceTypes = *

[rifle-amazon:chess]
rifleCapture = true
wallingRule = arrow

[passive-king-repro:chess]
blastPassiveTypes = n

[remove-king-repro:chess]
removeConnectN = 3

[passive-nonroyal-repro:chess]
castling = false
king = -
customPiece1 = k:K
extinctionValue = loss
extinctionPieceTypes = k
blastPassiveTypes = n

[death-petrify-repro:chess]
deathOnCaptureTypes = k
petrifyOnCaptureTypes = k

[push-gating:chess]
pushingStrength = R:8
gating = true
seirawanGating = true

[petrify-push:chess]
pushingStrength = R:8
petrifyOnCaptureTypes = R

[cylindrical-max-distance:chess]
cylindrical = true
customPiece1 = a:zR
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k

[cylindrical-ski-slip:chess]
cylindrical = true
customPiece1 = a:jR
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 8/8/8/8/8/8/8/Ap5p w - - 0 1

[cylindrical-push-direct:fairy]
cylindrical = true
maxFile = e
maxRank = 5
checking = false
castling = false
rook = r
pushingStrength = r:5
pushFirstColor = them
pushingRemoves = none
stepwisePushing = false
startFen = 5/5/5/5/rr2R w - - 0 1

[cylindrical-push-stepwise:fairy]
cylindrical = true
maxFile = e
maxRank = 5
checking = false
castling = false
rook = r
pushingStrength = r:5
pushFirstColor = them
pushChainEnemyOnly = true
pushingRemoves = none
stepwisePushing = true
startFen = 5/5/5/5/rr2R w - - 0 1

[torpedo-triple:chess]
enPassantTypes = p
tripleStepRegion = *(* *);

[custom-pawn-triple:chess]
customPiece1 = d:fmWfceF
pieceToCharTable = PNBRQ............D...Kpnbrq............d...k
pawnLikeTypes = d
enPassantTypes = d
tripleStepRegion = D(* *);
EOF
TEMP_INI="${FSX_TMP_INI}"

run_cmds() {
  local variant="$1"
  local vpath="$2"
  local cmds="$3"

  run_uci "$ENGINE" "$vpath" "$variant" <<<"$cmds"
}

echo "unorthodox interactions tests started"

# 0. Test triple-step for pieces in enPassantTypes
out=$(run_cmds "torpedo-triple" "${TEMP_INI}" "position fen 8/8/8/8/8/8/P7/K1k5 w - - 0 1
go perft 1")
echo "${out}" | grep -q "a2a3: 1"
echo "${out}" | grep -q "a2a4: 1"
echo "${out}" | grep -q "a2a5: 1"

# 0b. Test triple-step for custom pieces in enPassantTypes (demonstrates pseudo_legal bug)
out=$(run_cmds "custom-pawn-triple" "${TEMP_INI}" "position fen 8/8/8/8/8/8/D7/K1k5 w - - 0 1 moves a2a5
d")
if ! echo "${out}" | grep -q "Fen: 8/8/8/D7/8/8/8/K1k5 b"; then
  echo "Test 0b failed: move a2a5 was rejected or resulted in wrong FEN"
  echo "${out}" | grep "Fen:"
  exit 1
fi

# 1. Test rifleCapture + deathOnCaptureTypes
out=$(run_cmds "rifle-death" "${TEMP_INI}" "position fen 4k3/8/8/8/8/8/4q3/3QK3 w - - 0 1 moves d1e2
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/8/8/8/8/3\^K3 b"

# 2. Test rifleCapture + captureMorph
out=$(run_cmds "rifle-morph" "${TEMP_INI}" "position fen 4k3/8/8/8/8/8/4r3/3QK3 w - - 0 1 moves d1e2
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/8/8/8/8/3RK3 b"

# 3. Test rifleCapture + zero-range blast-on-capture (shooter survives)
out=$(run_cmds "rifle-atomic" "${TEMP_INI}" "position fen r3k3/8/8/8/8/8/8/R3K3 w - - 0 1 moves a1a8
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/8/8/8/8/R3K3 b"

# 4. Test rifleCapture + changingColorTrigger
out=$(run_cmds "rifle-color" "${TEMP_INI}" "position fen r3k3/8/8/8/8/8/8/R3K3 w - - 0 1 moves a1a8
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/8/8/8/8/r3K3 b"

# 5. Test rifleCapture + jumpCaptureTypes
out=$(run_cmds "rifle-jump" "${TEMP_INI}" "position fen 4k3/8/8/8/8/8/p7/M3K3 w - - 0 1 moves a1a3
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/8/8/8/8/M3K3 b"

# 5b. Test jumpCapture + zero-range blast-on-capture (mover survives)
out=$(run_cmds "jump-blast" "${TEMP_INI}" "position fen 4k3/8/8/8/8/8/m7/M3K3 w - - 0 1 moves a1a3
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/8/8/M7/8/4K3 b"

# 5c. Test mover-centered jump blast (mover is removed by the blast)
out=$(run_cmds "jump-blast-mover" "${TEMP_INI}" "position fen 4k3/8/8/8/8/8/m7/M3K3 w - - 0 1 moves a1a3
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/8/8/8/8/4K3 b"

# 5c. Test jumpCapture + blast + changingColor (mover changes color)
out=$(run_cmds "jump-blast-color" "${TEMP_INI}" "position fen 4k3/8/8/8/8/8/m7/M3K3 w - - 0 1 moves a1a3
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/8/8/m7/8/4K3 b"

# 6. Test rifleCapture + duck
out=$(run_cmds "rifle-duck" "${TEMP_INI}" "position fen p3k3/8/8/8/8/8/8/R3K3 w - - 0 1 moves a1a8,h1
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/8/8/8/8/R3K2\* b"

# 7. Test color-change
out=$(run_cmds "color-change" "${TEMP_INI}" "position fen 4k3/8/8/8/8/8/8/4R3 w - - 0 1 moves e1e2
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/8/8/8/4r3/8 b"

# 7b. Test captureMorph + changingColor undo order.
out=$(run_cmds "morph-color" "${TEMP_INI}" "position fen 4k3/8/8/3n4/4B3/8/8/4K3 w - - 0 1 moves e4d5
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/3n4/8/8/8/4K3 b"
out=$(run_cmds "morph-color" "${TEMP_INI}" "position fen 4k3/8/8/3n4/4B3/8/8/4K3 w - - 0 1
go perft 2")
grep -q "Nodes searched:" <<<"$out"

# 8. Test rifleCapture + captureForbidden
out=$(run_cmds "rifle-forbidden" "${TEMP_INI}" "position fen p3k3/8/8/8/8/8/8/R3K3 w - - 0 1 moves a1a8
d")
echo "${out}" | grep -q "Fen: p3k3/8/8/8/8/8/8/R3K3 w"

# 9. Test captureForbidden to king suppresses checks
out=$(run_cmds "forbidden-king-check" "${TEMP_INI}" "position startpos
d")
if echo "${out}" | grep -q "^Checkers: [^ ]"; then
  echo "captureForbidden king-check suppression failed"
  exit 1
fi

# 10. Test captureForbidden to king suppresses king captures in no-check variants
out=$(run_cmds "forbidden-king-capture" "${TEMP_INI}" "position startpos
go perft 1")
if echo "${out}" | grep -q "^a1a8: 1$"; then
  echo "captureForbidden king-capture suppression failed"
  exit 1
fi

# 11. Test ^ dead squares round-trip through FEN input/display
out=$(run_cmds "dead-fen" "${TEMP_INI}" "position fen 4k3/8/8/8/8/8/8/3^K3 w - - 0 1
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/8/8/8/8/3\^K3 w"

# 12. Test dead squares are capturable by either side
out=$(run_cmds "dead-fen" "${TEMP_INI}" "position fen 4k3/8/8/8/8/8/8/3^R2K w - - 0 1
go perft 1")
echo "${out}" | grep -q "^e1d1: 1$"
out=$(run_cmds "dead-fen" "${TEMP_INI}" "position fen r3k3/8/8/8/8/8/8/3^3K b - - 0 1
go perft 1")
echo "${out}" | grep -q "^a8d8: 1$"

# 13. Test iron-like uncapturable pieces via captureForbidden alongside extinction targets
out=$(run_cmds "iron-extinction" "${TEMP_INI}" "position startpos
go perft 1")
echo "${out}" | grep -q "^e1e2: 1$"
if echo "${out}" | grep -q "^e2e1:"; then
  echo "captureForbidden iron-piece suppression failed"
  exit 1
fi

# 14. Test the same setup from black's side: enemy queen still cannot capture the iron piece
out=$(run_cmds "iron-extinction" "${TEMP_INI}" "position fen 4k3/8/8/8/8/8/3Qq3/4S2K b - - 0 1
go perft 1")
if echo "${out}" | grep -q "^e2e1:"; then
  echo "captureForbidden iron-piece suppression for black failed"
  exit 1
fi

# 15. Test rifleCapture + gating (ensure piece is NOT overwritten)
# Move White King from e1 to e2 (rifle capture). It should NOT gate on e1.
out=$(run_cmds "rifle-gating" "${TEMP_INI}" "position fen r3k2r/8/8/8/8/8/4q3/4K3[B] w KQkq - 0 1
go perft 1")
if echo "${out}" | grep -q "e1e2b:"; then
  echo "rifleCapture + gating bug: gating move was generated"
  exit 1
fi

# 15b. Test rifleCapture + symmetric gating still allows quiet gating moves
out=$(run_cmds "rifle-symgating" "${TEMP_INI}" "position fen 4k3/8/8/8/8/8/8/4K3[RR] w ABCDEFGH - 0 1
go perft 1")
if ! echo "${out}" | grep -q "^e1e2r,d1: 1$"; then
  echo "rifleCapture + symmetric gating bug: quiet gating move was suppressed"
  exit 1
fi
if ! grep -Fxq "Nodes searched: 9" <<<"$out"; then
  echo "rifleCapture + symmetric gating bug: expected 9 nodes"
  exit 1
fi

# 16. Test surroundCapture + changingColor (ensure color change triggers on bycatch)
# White Rook moves from e2 to e3, between black pawns on d3 and f3.
out=$(run_cmds "surround-color" "${TEMP_INI}" "position fen 4k3/8/8/8/8/3p1p2/4R3/8 w - - 0 1 moves e2e3
d")
if ! echo "${out}" | grep -q "Fen: 4k3/8/8/8/8/4r3/8/8 b"; then
  echo "surroundCapture + changingColor bug: color change did not trigger"
  exit 1
fi

# 17. Test rifleCapture + arrow walling (ensure arrow shot from origin)
# White Queen at e1, Black Pawn at e2. rifle capture e1e2, shoot arrow to e3.
# h5 is NOT reachable from e1 (origin) but IS from e2 (if it was moved there).
# e1-h5: 7 files, 4 ranks (NO). e2-h5: 3 files, 3 ranks (YES).
out=$(run_cmds "rifle-amazon" "${TEMP_INI}" "position fen 4k3/8/8/7p/8/8/4p3/4Q1K1 w - - 0 1
go perft 1")
if echo "${out}" | grep -q "e1e2,h5"; then
  echo "rifleCapture + arrow walling bug: arrow shot from victim square"
  exit 1
fi

# 18. Test blastPassiveTypes + royal kings remains loadable and behaves as a passive check rule.
out=$(run_cmds "passive-king-repro" "${TEMP_INI}" "d")
if ! echo "${out}" | grep -q "info string variant passive-king-repro"; then
  echo "blastPassiveTypes + royal kings variant should remain loadable"
  exit 1
fi

# 19. Test removeConnectN + royal kings rejects the variant
out=$(run_cmds "remove-king-repro" "${TEMP_INI}" "d")
echo "${out}" | grep -q "info string unknown variant 'remove-king-repro'"
if echo "${out}" | grep -q "info string variant remove-king-repro"; then
  echo "removeConnectN + royal kings variant should have been rejected"
  exit 1
fi

# 20. Test non-royal custom extinction setup still loads with blastPassiveTypes
out=$(run_cmds "passive-nonroyal-repro" "${TEMP_INI}" "d")
if ! echo "${out}" | grep -q "info string variant passive-nonroyal-repro"; then
  echo "non-royal blastPassiveTypes setup should remain loadable"
  exit 1
fi

# 21. Test deathOnCaptureTypes + petrifyOnCaptureTypes rejects the variant
out=$(run_cmds "death-petrify-repro" "${TEMP_INI}" "d")
if ! echo "${out}" | grep -q "info string unknown variant 'death-petrify-repro'"; then
  echo "deathOnCaptureTypes + petrifyOnCaptureTypes variant should have been rejected"
  exit 1
fi
if echo "${out}" | grep -q "info string variant death-petrify-repro"; then
  echo "deathOnCaptureTypes + petrifyOnCaptureTypes variant should have been rejected"
  exit 1
fi

# 22. Test pushingStrength + gating.
# The rook pushes the pawn from d1 to f1, and the gated knight lands on a1.
out=$(run_cmds "push-gating" "${TEMP_INI}" "position fen n3k2n/8/8/8/8/8/8/R2p3K[N] w Qkq - 0 1 moves a1e1n
d")
if ! echo "${out}" | grep -q "Fen: n3k2n/8/8/8/8/8/8/N3Rp1K\\[\\] b  - 1 1"; then
  echo "pushingStrength + gating bug: piece was not correctly pushed"
  exit 1
fi

# 23. Test pushingStrength + petrifyOnCaptureTypes
out=$(run_cmds "petrify-push" "${TEMP_INI}" "position fen K3k3/8/8/8/8/8/8/R6p w - - 0 1 moves a1h1
d")
if ! echo "${out}" | grep -q "Fen: K3k3/8/8/8/8/8/8/7\\* b"; then
  echo "pushingStrength + petrifyOnCapture bug: pushed offboard piece did not petrify"
  exit 1
fi

# 24. Test cylindrical + max distance accepts the variant and reaches the wrapped ray endpoint.
out=$(run_cmds "cylindrical-max-distance" "${TEMP_INI}" "position fen 8/8/8/8/8/8/8/A7 w - - 0 1
go perft 1")
echo "${out}" | grep -q "^a1b1: 1$"
echo "${out}" | grep -q "^a1h1: 1$"
! echo "${out}" | grep -q "^a1c1:"

# 25. Test cylindrical push across the file edge with both direct and stepwise analyzers.
out=$(run_cmds "cylindrical-push-direct" "${TEMP_INI}" "position startpos
go perft 1")
echo "${out}" | grep -q "^e1a1: 1$"
! echo "${out}" | grep -q "^e1a1: 2$"

out=$(run_cmds "cylindrical-push-direct" "${TEMP_INI}" "position fen 5/5/5/5/rrrrR w - - 0 1
go perft 1")
! echo "${out}" | grep -q "^e1a1:"

out=$(run_cmds "cylindrical-push-stepwise" "${TEMP_INI}" "position startpos
go perft 1")
echo "${out}" | grep -q "^e1a1: 1$"

out=$(run_cmds "cylindrical-push-stepwise" "${TEMP_INI}" "position fen 5/5/5/5/rrrrR w - - 0 1
go perft 1")
! echo "${out}" | grep -q "^e1a1:"

# 26. Test cylindrical + ski-slip skips the first square (and is blocked by it)
out=$(run_cmds "cylindrical-ski-slip" "${TEMP_INI}" "position startpos
go perft 1")
if echo "${out}" | grep -q "a1b1:"; then
  echo "cylindrical + ski-slip bug: captured adjacent blocked square"
  exit 1
fi
if echo "${out}" | grep -q "a1h1:"; then
  echo "cylindrical + ski-slip bug: captured adjacent blocked square (wrap)"
  exit 1
fi
if echo "${out}" | grep -q "a1c1:"; then
  echo "cylindrical + ski-slip bug: leaped over blocked square"
  exit 1
fi

rm "${TEMP_INI}"

echo "unorthodox interactions tests passed"
