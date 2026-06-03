#!/bin/bash
# Petrifying capture transfer regression tests

set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "petrify-transfer test"

load_inline_variants <<'EOF'
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
tmp_ini="${FSX_TMP_INI}"

echo "petrify-transfer testing started"

check_out=$("${ENGINE}" check "${tmp_ini}" 2>&1)
echo "${check_out}" > /dev/null

out=$(run_uci "$ENGINE" "$tmp_ini" petrihouse-control <<'CMDS'
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
CMDS
)

grep -Fq "Fen: 4k3/8/8/3*4/8/8/8/4K3[P] b - - 0 1" <<<"$out"
grep -Fq "Fen: 4k3/8/8/3*4/8/8/8/4K3[NP] b - - 0 1" <<<"$out"
test "$(grep -Fc "Fen: 4k3/8/8/3*4/8/8/8/4K3[] b - - 0 1" <<<"$out")" -eq 2

echo "petrify-transfer testing OK"
