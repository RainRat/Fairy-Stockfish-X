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

TMP_VARIANT_PATH=$(mktemp "${TMPDIR:-/tmp}/fsx-piece-step-regions-XXXXXX")
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

[ipawnlike-piece-specific:chess]
customPiece1 = a:iW
pieceToCharTable = A:a
pawnLikeTypes = a
startFen = 4k3/8/8/8/8/8/8/4A2K w - - 0 1
doubleStepRegionWhite = A(e1); *(*2)

[irider-roundtrip:chess]
king = k
customPiece1 = d:efWfFmsWifmnD
pieceToCharTable = PNBRQ............D...Kpnbrq............d...k
pawnLikeTypes = d
enPassantTypes = d
startFen = 4k3/8/8/8/8/8/8/4D2K w - - 0 1
doubleStepRegionWhite = D(e1); *(*2)
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

run_position() {
  local variant="$1"
  local cmds="$2"
  cat <<CMDS | "${ENGINE}"
uci
setoption name VariantPath value ${TMP_VARIANT_PATH}
setoption name UCI_Variant value ${variant}
${cmds}
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

out=$(run_perft "ipawnlike-piece-specific")
echo "${out}" | grep -q "^e1e2: 1$"
echo "${out}" | grep -q "^e1e3: 1$"
! echo "${out}" | grep -q "^e1e4: 1$"

out=$(run_position "irider-roundtrip" "position fen 4k3/8/8/8/8/8/8/4D2K w - - 0 1 moves e1d1 e8e7 d1e1 e7e8
go perft 1")
echo "${out}" | grep -q "^e1e2: 1$"
! echo "${out}" | grep -q "^e1e3: 1$"

rm -f "${TMP_VARIANT_PATH}"
unset TMP_VARIANT_PATH

echo "piece-specific step region regression tests passed"
