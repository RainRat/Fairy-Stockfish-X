#!/bin/bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "in-place transform undo test"

load_inline_variants <<'EOF'
[capture-morph-color:chess]
captureMorph = true
changingColorTrigger = capture
changingColorPieceTypes = *

[move-morph-color:chess]
moveMorphPieceType = b:n
changingColorTrigger = always
changingColorPieceTypes = n

[move-morph-promoted:chess]
capturesToHand = true
moveMorphPieceType = b:n
EOF
TEMP_INI="${FSX_TMP_INI}"

echo "in-place transform undo tests started"

# captureMorph applies before changingColor, so undo must restore color first,
# then the original mover type. Perft 2 exercises do/undo on the composed move.
out=$(run_uci "$ENGINE" "$TEMP_INI" "capture-morph-color" <<'CMDS'
position fen 4k3/8/8/3n4/4B3/8/8/4K3 w - - 0 1 moves e4d5
d
CMDS
)
echo "${out}" | grep -q "Fen: 4k3/8/8/3n4/8/8/8/4K3 b"
out=$(run_uci "$ENGINE" "$TEMP_INI" "capture-morph-color" <<'CMDS'
position fen 4k3/8/8/3n4/4B3/8/8/4K3 w - - 0 1
go perft 2
CMDS
)
grep -q "Nodes searched:" <<<"$out"

# moveMorph can also compose with changingColor on the same mover.
out=$(run_uci "$ENGINE" "$TEMP_INI" "move-morph-color" <<'CMDS'
position fen 4k3/8/8/8/8/8/8/2B1K3 w - - 0 1 moves c1g5
d
CMDS
)
echo "${out}" | grep -q "Fen: 4k3/8/8/6n1/8/8/8/4K3 b"
out=$(run_uci "$ENGINE" "$TEMP_INI" "move-morph-color" <<'CMDS'
position fen 4k3/8/8/8/8/8/8/2B1K3 w - - 0 1
go perft 2
CMDS
)
grep -q "Nodes searched:" <<<"$out"

# A promoted piece that morphs on move must restore its promoted state after
# the full do/undo cycle exercised by perft.
out=$(run_uci "$ENGINE" "$TEMP_INI" "move-morph-promoted" <<'CMDS'
position fen 4k3/8/8/8/8/8/8/2B~1K3[] w - - 0 1
go perft 1
d
CMDS
)
echo "${out}" | grep -q "~B"

echo "in-place transform undo tests passed"
