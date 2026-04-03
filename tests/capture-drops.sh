#!/bin/bash

set -euo pipefail

error() {
  echo "capture-drops regression failed on line $1"
  [[ -n "${TMP_VARIANT_PATH:-}" ]] && rm -f "${TMP_VARIANT_PATH}"
  exit 1
}
trap 'error ${LINENO}' ERR

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
ENGINE="${1:-${REPO_ROOT}/src/stockfish}"

TMP_VARIANT_PATH=$(mktemp /tmp/fsx-capture-drops-XXXXXX.ini)
cat >"${TMP_VARIANT_PATH}" <<'INI'
[capture-drop-control:chess]
captureType = hand
pieceDrops = true
pocketSize = 6
startFen = 4k3/8/8/4p3/8/8/8/4K3[Q] w - - 0 1

[capture-drop:capture-drop-control]
captureDrops = q

[capture-drop-self:capture-drop]
selfCapture = true
startFen = 4k3/8/8/8/4P3/8/8/4K3[Q] w - - 0 1
INI

run_perft() {
  local variant="$1"
  cat <<CMDS | "${ENGINE}"
uci
setoption name VariantPath value ${TMP_VARIANT_PATH}
setoption name UCI_Variant value ${variant}
position startpos
go perft 1
quit
CMDS
}

run_display() {
  local variant="$1"
  local moves="$2"
  cat <<CMDS | "${ENGINE}"
uci
setoption name VariantPath value ${TMP_VARIANT_PATH}
setoption name UCI_Variant value ${variant}
position startpos moves ${moves}
d
quit
CMDS
}

echo "capture-drops regression tests started"

out=$(run_perft "capture-drop-control")
! echo "${out}" | grep -q "^Q@e5: 1$"

out=$(run_perft "capture-drop")
echo "${out}" | grep -q "^Q@e5: 1$"

out=$(run_display "capture-drop" "Q@e5")
echo "${out}" | grep -Fq "Fen: 4k3/8/8/4Q3/8/8/8/4K3[P] b - - 0 1"

out=$(run_perft "capture-drop-self")
echo "${out}" | grep -q "^Q@e4: 1$"

rm -f "${TMP_VARIANT_PATH}"
unset TMP_VARIANT_PATH

echo "capture-drops regression tests passed"