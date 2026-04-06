#!/bin/bash

set -euo pipefail

error() {
  echo "unorthodox interactions test failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-./stockfish}
VARIANT_PATH=${2:-src/variants.ini}

run_cmds() {
  local variant="$1"
  local vpath="$2"
  local cmds="$3"
  cat <<CMDS | "${ENGINE}" 2>&1
uci
setoption name VariantPath value ${vpath}
setoption name UCI_Variant value ${variant}
${cmds}
quit
CMDS
}

# Create a temporary ini file for testing
TEMP_INI=$(mktemp)
cat <<EOF > "${TEMP_INI}"
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
king = -
customPiece1 = k:K
extinctionValue = -VALUE_MATE
extinctionPieceTypes = k

[rifle-color:chess]
rifleCapture = true
changingColorTrigger = capture
changingColorPieceTypes = *

[rifle-jump:chess]
customPiece1 = m:D
jumpCaptureTypes = m
rifleCapture = true

[rifle-duck:chess]
rifleCapture = true
wallingRule = duck
king = -
customPiece1 = k:K
extinctionValue = -VALUE_MATE
extinctionPieceTypes = k

[king-color:chess]
changingColorTrigger = always
changingColorPieceTypes = *

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

[surround-color:chess]
surroundCaptureIntervene = true
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
king = -
customPiece1 = k:K
extinctionValue = -VALUE_MATE
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
EOF

echo "unorthodox interactions tests started"

# 1. Test rifleCapture + deathOnCaptureTypes
out=$(run_cmds "rifle-death" "${TEMP_INI}" "position fen 4k3/8/8/8/8/8/4q3/3QK3 w - - 0 1 moves d1e2
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/8/8/8/8/3\^K3 b"

# 2. Test rifleCapture + captureMorph
out=$(run_cmds "rifle-morph" "${TEMP_INI}" "position fen 4k3/8/8/8/8/8/4r3/3QK3 w - - 0 1 moves d1e2
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/8/8/8/8/3RK3 b"

# 3. Test rifleCapture + zero-range blast-on-capture
out=$(run_cmds "rifle-atomic" "${TEMP_INI}" "position fen r3k3/8/8/8/8/8/8/R3K3 w - - 0 1 moves a1a8
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/8/8/8/8/4K3 b"

# 4. Test rifleCapture + changingColorTrigger
out=$(run_cmds "rifle-color" "${TEMP_INI}" "position fen r3k3/8/8/8/8/8/8/R3K3 w - - 0 1 moves a1a8
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/8/8/8/8/r3K3 b"

# 5. Test rifleCapture + jumpCaptureTypes
out=$(run_cmds "rifle-jump" "${TEMP_INI}" "position fen 4k3/8/8/8/8/8/p7/M3K3 w - - 0 1 moves a1a3
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/8/8/8/8/M3K3 b"

# 6. Test rifleCapture + duck
out=$(run_cmds "rifle-duck" "${TEMP_INI}" "position fen p3k3/8/8/8/8/8/8/R3K3 w - - 0 1 moves a1a8,h1
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/8/8/8/8/R3K2\* b"

# 7. Test king-color-change
out=$(run_cmds "king-color" "${TEMP_INI}" "position fen 4k3/8/8/8/8/8/8/4K3 w - - 0 1 moves e1e2
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/8/8/8/4k3/8 b"

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

# 16. Test surroundCapture + changingColor (ensure color change triggers on bycatch)
# White King moves from e2 to e3, between black pawns on d3 and f3.
out=$(run_cmds "surround-color" "${TEMP_INI}" "position fen 4k3/8/8/8/8/3p1p2/4K3/8 w - - 0 1 moves e2e3
d")
if ! echo "${out}" | grep -q "Fen: 4k3/8/8/8/8/4k3/8/8 b"; then
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

# 18. Test blastPassiveTypes + royal kings rejects the variant
out=$(run_cmds "passive-king-repro" "${TEMP_INI}" "d")
echo "${out}" | grep -q "Can not use kings with blastPassiveTypes."
if echo "${out}" | grep -q "info string variant passive-king-repro"; then
  echo "blastPassiveTypes + royal kings variant should have been rejected"
  exit 1
fi

# 19. Test removeConnectN + royal kings rejects the variant
out=$(run_cmds "remove-king-repro" "${TEMP_INI}" "d")
echo "${out}" | grep -q "Can not use kings with removeConnectN."
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

# 22. Test pushingStrength + gating (ensure gating correctly pushes)
out=$(run_cmds "push-gating" "${TEMP_INI}" "position fen n3k2n/8/8/8/8/8/8/R2p3K[N] w Qkq - 0 1 moves a1e1n
d")
if ! echo "${out}" | grep -q "Fen: n3k2n/8/8/8/8/8/8/N2pR2K"; then
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

# 24. Test cylindrical + max distance rejects the variant
out=$("${ENGINE}" check "${TEMP_INI}" 2>&1)
if ! echo "${out}" | grep -q "Wrapped boards do not support x/z rider modifiers"; then
  echo "cylindrical + x/z rider variant should have been rejected (warned)"
  exit 1
fi

# 25. Test cylindrical + ski-slip skips the first square (and is blocked by it)
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
