#!/bin/bash

set -euo pipefail

error() {
  echo "in-place transform undo test failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
ENGINE=${1:-"${ROOT_DIR}/src/stockfish"}

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

TEMP_INI=$(mktemp)
trap 'rm -f "${TEMP_INI}"' EXIT

cat <<EOF > "${TEMP_INI}"
[capture-morph-color:chess]
captureMorph = true
changingColorTrigger = capture
changingColorPieceTypes = *

[move-morph-color:chess]
moveMorphPieceType = b:n
changingColorTrigger = always
changingColorPieceTypes = n
EOF

echo "in-place transform undo tests started"

# captureMorph applies before changingColor, so undo must restore color first,
# then the original mover type. Perft 2 exercises do/undo on the composed move.
out=$(run_cmds "capture-morph-color" "${TEMP_INI}" "position fen 4k3/8/8/3n4/4B3/8/8/4K3 w - - 0 1 moves e4d5
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/3n4/8/8/8/4K3 b"
out=$(run_cmds "capture-morph-color" "${TEMP_INI}" "position fen 4k3/8/8/3n4/4B3/8/8/4K3 w - - 0 1
go perft 2")
grep -q "Nodes searched:" <<<"$out"

# moveMorph can also compose with changingColor on the same mover.
out=$(run_cmds "move-morph-color" "${TEMP_INI}" "position fen 4k3/8/8/8/8/8/8/2B1K3 w - - 0 1 moves c1g5
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/6n1/8/8/8/4K3 b"
out=$(run_cmds "move-morph-color" "${TEMP_INI}" "position fen 4k3/8/8/8/8/8/8/2B1K3 w - - 0 1
go perft 2")
grep -q "Nodes searched:" <<<"$out"

echo "in-place transform undo tests passed"
