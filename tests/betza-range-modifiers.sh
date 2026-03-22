#!/bin/bash

set -euo pipefail

error() {
  echo "betza range modifiers test failed on line $1"
  [[ -n "${TMP_VARIANT_PATH:-}" ]] && rm -f "${TMP_VARIANT_PATH}"
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-./stockfish}

TMP_VARIANT_PATH=$(mktemp /tmp/fsx-betza-range-XXXXXX.ini)
cat >"${TMP_VARIANT_PATH}" <<'INI'
[range35:chess]
king = -
checking = false
customPiece1 = a:R[3-5]
pieceToCharTable = A:a
startFen = 8/8/8/8/4A3/8/8/8 w - - 0 1

[range3plus:chess]
king = -
checking = false
customPiece1 = a:R[3-]
pieceToCharTable = A:a
startFen = 8/8/8/8/4A3/8/8/8 w - - 0 1

[rangeinvalid:chess]
king = -
checking = false
customPiece1 = a:R[3]
pieceToCharTable = A:a
startFen = 8/8/8/8/4A3/8/8/8 w - - 0 1
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

echo "betza range modifiers tests started"

out=$(run_perft "range35")
echo "${out}" | grep -q "^e4e7: 1$"
echo "${out}" | grep -q "^e4e8: 1$"
echo "${out}" | grep -q "^e4b4: 1$"
echo "${out}" | grep -q "^e4h4: 1$"
! echo "${out}" | grep -q "^e4e5: 1$"
! echo "${out}" | grep -q "^e4e6: 1$"
! echo "${out}" | grep -q "^e4d4: 1$"
! echo "${out}" | grep -q "^e4c4: 1$"

out=$(run_perft "range3plus")
echo "${out}" | grep -q "^e4e7: 1$"
echo "${out}" | grep -q "^e4e8: 1$"
echo "${out}" | grep -q "^e4b4: 1$"
echo "${out}" | grep -q "^e4h4: 1$"
! echo "${out}" | grep -q "^e4e5: 1$"
! echo "${out}" | grep -q "^e4e6: 1$"
! echo "${out}" | grep -q "^e4d4: 1$"
! echo "${out}" | grep -q "^e4c4: 1$"

invalid_out=$(cat <<CMDS | "${ENGINE}" 2>&1
uci
setoption name VariantPath value ${TMP_VARIANT_PATH}
setoption name UCI_Variant value rangeinvalid
quit
CMDS
)

echo "${invalid_out}" | grep -q "Invalid Betza rider range"

rm -f "${TMP_VARIANT_PATH}"
unset TMP_VARIANT_PATH

echo "betza range modifiers tests passed"
