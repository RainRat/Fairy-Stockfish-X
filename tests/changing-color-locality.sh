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
castling = false
king = -
customPiece1 = u:R
customPiece2 = v:N
customPiece3 = k:K
blastPassiveTypes = u
changingColorTrigger = capture
changingColorPieceTypes = v
pieceToCharTable = PNBRQ............UV..Kpnbrq............uv..k
INI

out=$(
  python3 - <<'PY' "${TMP_VARIANT_PATH}"
import sys

import pyffish as sf

variant_path = sys.argv[1]
with open(variant_path, "r", encoding="utf-8") as f:
    sf.load_variant_config(f.read())

print(sf.get_fen("surround-color", "4k3/8/8/8/8/3p1p2/4K3/8 w - - 0 1", ["e2e3"]))
PY
)
grep -q "^4k3/8/8/8/8/4k3/8/8 b - - 1 1$" <<<"${out}"

out=$(
  python3 - <<'PY' "${TMP_VARIANT_PATH}"
import sys

import pyffish as sf

variant_path = sys.argv[1]
with open(variant_path, "r", encoding="utf-8") as f:
    sf.load_variant_config(f.read())

print(sf.get_fen("remote-burner-color", "8/8/8/8/8/8/1p6/U3V3 w - - 0 1", ["e1g2"]))
PY
)
if grep -q "^8/8/8/8/8/8/6n1/U7 b - - 1 1$" <<<"${out}"; then
  echo "remote passive burner incorrectly triggered changingColor"
  exit 1
fi
grep -q "^8/8/8/8/8/8/6V1/U7 b - - 1 1$" <<<"${out}"

rm -f "${TMP_VARIANT_PATH}"
unset TMP_VARIANT_PATH

echo "changing-color locality regression passed"
