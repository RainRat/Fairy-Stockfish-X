#!/bin/bash

set -euo pipefail

error() {
  echo "changing-color locality regression failed on line $1"
  [[ -n "${TMP_VARIANT_PATH:-}" ]] && rm -f "${TMP_VARIANT_PATH}"
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-./stockfish}

TMP_VARIANT_PATH=$(mktemp /tmp/fsx-changing-color-locality-XXXXXX.ini)
cat >"${TMP_VARIANT_PATH}" <<'INI'
[surround-color:chess]
surroundCaptureIntervene = true
changingColorTrigger = capture
changingColorPieceTypes = *

[remote-burner-color:chess]
customPiece1 = u:R
customPiece2 = v:N
blastPassiveTypes = u
changingColorTrigger = capture
changingColorPieceTypes = v
INI

out=$(
  cat <<CMDS | "${ENGINE}"
uci
setoption name VariantPath value ${TMP_VARIANT_PATH}
setoption name UCI_Variant value surround-color
position fen 4k3/8/8/8/8/3p1p2/4K3/8 w - - 0 1 moves e2e3
d
quit
CMDS
)
grep -q "Fen: 4k3/8/8/8/8/4k3/8/8 b" <<<"${out}"

out=$(
  cat <<CMDS | "${ENGINE}"
uci
setoption name VariantPath value ${TMP_VARIANT_PATH}
setoption name UCI_Variant value remote-burner-color
position fen 8/8/8/8/8/8/1p6/U3V3 w - - 0 1 moves e1g2
d
quit
CMDS
)
if grep -q "Fen: 8/8/8/8/8/8/6n1/U7 b" <<<"${out}"; then
  echo "remote passive burner incorrectly triggered changingColor"
  exit 1
fi
grep -q "Fen: 8/8/8/8/8/8/6V1/U7 b" <<<"${out}"

rm -f "${TMP_VARIANT_PATH}"
unset TMP_VARIANT_PATH

echo "changing-color locality regression passed"
