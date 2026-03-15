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
EOF

echo "unorthodox interactions tests started"

# 1. Test rifleCapture + deathOnCaptureTypes
out=$(run_cmds "rifle-death" "${TEMP_INI}" "position fen 4k3/8/8/8/8/8/4q3/3QK3 w - - 0 1 moves d1e2
d")
echo "${out}" | grep -q "^Fen: 4k3/8/8/8/8/8/8/3\^K3 b"

# 2. Test rifleCapture + captureMorph
out=$(run_cmds "rifle-morph" "${TEMP_INI}" "position fen 4k3/8/8/8/8/8/4r3/3QK3 w - - 0 1 moves d1e2
d")
echo "${out}" | grep -q "^Fen: 4k3/8/8/8/8/8/8/3RK3 b"

# 3. Test rifleCapture + capturerDiesOnCapture
out=$(run_cmds "rifle-atomic" "${TEMP_INI}" "position fen r3k3/8/8/8/8/8/8/R3K3 w - - 0 1 moves a1a8
d")
echo "${out}" | grep -q "^Fen: 4k3/8/8/8/8/8/8/4K3 b"

rm "${TEMP_INI}"

echo "unorthodox interactions tests passed"
