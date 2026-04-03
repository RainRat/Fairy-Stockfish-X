#!/bin/bash

set -euo pipefail

error() {
  echo "changing-color drop-capture regression failed on line $1"
  [[ -n "${TMP_VARIANT_PATH:-}" ]] && rm -f "${TMP_VARIANT_PATH}"
  exit 1
}
trap 'error ${LINENO}' ERR

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
ENGINE="${1:-${REPO_ROOT}/src/stockfish}"

TMP_VARIANT_PATH=$(mktemp /tmp/fsx-changing-color-drop-capture-XXXXXX.ini)
cat >"${TMP_VARIANT_PATH}" <<'INI'
[capture-drop-color:chess]
customPiece1 = u:W
pieceDrops = true
captureDrops = u
changingColorTrigger = capture
changingColorPieceTypes = u
INI

out=$(
  cat <<CMDS | "${ENGINE}"
uci
setoption name VariantPath value ${TMP_VARIANT_PATH}
setoption name UCI_Variant value capture-drop-color
position fen 7k/8/8/8/4p3/8/8/7K[U] w - - 0 1 moves U@e4
d
quit
CMDS
)

grep -q "Fen: 7k/8/8/8/4u3/8/8/7K\\[\\] b" <<<"${out}"

rm -f "${TMP_VARIANT_PATH}"
unset TMP_VARIANT_PATH

echo "changing-color drop-capture regression passed"