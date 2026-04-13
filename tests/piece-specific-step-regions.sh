#!/bin/bash

set -euo pipefail

error() {
  echo "piece-specific step region regression failed on line $1"
  [[ -n "${TMP_VARIANT_PATH:-}" ]] && rm -f "${TMP_VARIANT_PATH}"
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-}
if [[ -z "${ENGINE}" ]]; then
  if [[ -x "src/stockfish" ]]; then
    ENGINE="src/stockfish"
  else
    ENGINE="./stockfish"
  fi
fi

TMP_VARIANT_PATH=$(mktemp /tmp/fsx-piece-step-regions-XXXXXX.ini)
cat >"${TMP_VARIANT_PATH}" <<'INI'
[istep-piece-specific:chess]
king = -
checking = false
customPiece1 = a:iW
pieceToCharTable = A:a
startFen = 8/8/8/8/8/8/8/4A3 w - - 0 1
doubleStepRegionWhite = A(e1); *(*2)

[irider-piece-specific:chess]
king = -
checking = false
customPiece1 = a:imR2
pieceToCharTable = A:a
startFen = 8/8/8/8/8/8/8/4A3 w - - 0 1
doubleStepRegionWhite = A(e1); *(*2)

[itriple-piece-specific:chess]
king = -
checking = false
customPiece1 = a:iW
pieceToCharTable = A:a
startFen = 8/8/8/8/8/8/8/4A3 w - - 0 1
tripleStepRegionWhite = A(e1)
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

echo "piece-specific step region regression tests started"

out=$(run_perft "istep-piece-specific")
echo "${out}" | grep -q "^e1e2: 1$"
echo "${out}" | grep -q "^e1e3: 1$"
! echo "${out}" | grep -q "^e1e4: 1$"

out=$(run_perft "irider-piece-specific")
echo "${out}" | grep -q "^e1e2: 1$"
echo "${out}" | grep -q "^e1e3: 1$"

out=$(run_perft "itriple-piece-specific")
echo "${out}" | grep -q "^e1e2: 1$"
echo "${out}" | grep -q "^e1e3: 1$"
echo "${out}" | grep -q "^e1e4: 1$"

rm -f "${TMP_VARIANT_PATH}"
unset TMP_VARIANT_PATH

echo "piece-specific step region regression tests passed"
