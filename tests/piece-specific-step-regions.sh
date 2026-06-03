#!/bin/bash

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "piece-specific step region regression"

load_inline_variants <<'INI'
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

[semitorpedo-test:chess]
doubleStepRegionWhite = *2 *3
doubleStepRegionBlack = *7 *6
startFen = rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1
INI
TMP_VARIANT_PATH="${FSX_TMP_INI}"

run_perft() {
  local variant="$1"

  run_uci "$ENGINE" "$TMP_VARIANT_PATH" "$variant" <<'UCI'
position startpos
go perft 1
UCI
}

run_position() {
  local variant="$1"
  local cmds="$2"

  run_uci "$ENGINE" "$TMP_VARIANT_PATH" "$variant" <<<"$cmds"
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

# Semitorpedo double-step test: pawn can double-step from 3rd rank even after moving
out=$(run_position "semitorpedo-test" "position startpos moves e2e3 a7a6
go perft 1")
echo "${out}" | grep -q "^e3e4: 1$"
echo "${out}" | grep -q "^e3e5: 1$"

# Negative test: pawn outside doubleStepRegion cannot double-step
out=$(run_position "semitorpedo-test" "position startpos moves e2e4 a7a6
go perft 1")
echo "${out}" | grep -q "^e4e5: 1$"
! echo "${out}" | grep -q "^e4e6: 1$"

echo "piece-specific step region regression tests passed"
