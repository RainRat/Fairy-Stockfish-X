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
  cat <<CMDS | "${ENGINE}"
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
capturerDiesOnCapture = true
blastOnCapture = true
rifleCapture = true

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

# 3. Test rifleCapture + capturerDiesOnCapture
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

rm "${TEMP_INI}"

echo "unorthodox interactions tests passed"
