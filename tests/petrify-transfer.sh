#!/bin/bash
# Petrifying capture transfer regression tests

set -euo pipefail

error() {
  echo "petrify-transfer testing failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
ENGINE="${1:-${REPO_ROOT}/src/stockfish}"

echo "petrify-transfer testing started"

cfg=$(mktemp)
out=$(mktemp)
cleanup() {
  rm -f "$cfg" "$out"
}
trap cleanup EXIT

cat > "$cfg" <<'EOF'
[petrihouse:chess]
captureType = hand
pieceDrops = true
pocketSize = 6
petrifyOnCaptureTypes = q
petrifyOnCaptureSuppressTransfer = true

[petrihouse-control:petrihouse]
petrifyOnCaptureSuppressTransfer = false

[petriatomic:atomic]
captureType = hand
pieceDrops = true
pocketSize = 6
dropChecks = false
castling = false
petrifyOnCaptureTypes = q
petrifyOnCaptureSuppressTransfer = true

[petriatomic-control:petriatomic]
petrifyOnCaptureSuppressTransfer = false
EOF

"$ENGINE" check "$cfg" > "$out" 2>&1

cat <<CMDS | "$ENGINE" > "$out" 2>&1
uci
setoption name VariantPath value $cfg
setoption name UCI_Variant value petrihouse-control
position fen 4k3/8/8/3p4/4Q3/8/8/4K3[] w - - 0 1 moves e4d5
d
setoption name UCI_Variant value petrihouse
position fen 4k3/8/8/3p4/4Q3/8/8/4K3[] w - - 0 1 moves e4d5
d
setoption name UCI_Variant value petriatomic-control
position fen 4k3/8/3n4/3p4/4Q3/8/8/4K3[] w - - 0 1 moves e4d5
d
setoption name UCI_Variant value petriatomic
position fen 4k3/8/3n4/3p4/4Q3/8/8/4K3[] w - - 0 1 moves e4d5
d
quit
CMDS

grep -Fq "Fen: 4k3/8/8/3*4/8/8/8/4K3[P] b - - 0 1" "$out"
grep -Fq "Fen: 4k3/8/8/3*4/8/8/8/4K3[NP] b - - 0 1" "$out"
test "$(grep -Fc "Fen: 4k3/8/8/3*4/8/8/8/4K3[] b - - 0 1" "$out")" -eq 2

echo "petrify-transfer testing OK"