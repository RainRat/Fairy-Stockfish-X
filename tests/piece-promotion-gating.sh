#!/bin/bash

set -euo pipefail

error() {
  echo "piece promotion gating regression failed on line $1"
  [[ -n "${TMP_VARIANT_PATH:-}" ]] && rm -f "${TMP_VARIANT_PATH}"
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-./stockfish}

TMP_VARIANT_PATH=$(mktemp "${TMPDIR:-/tmp}/fsx-promowall-XXXXXX")
cat >"${TMP_VARIANT_PATH}" <<'INI'
[promowall:chess]
wallingRule = past
promotedPieceType = n:q
promotionRegionWhite = *8
promotionRegionBlack = *1
mandatoryPiecePromotion = true
startFen = 4k3/1N6/8/8/8/8/8/4K3 w - - 0 1

[promowall-split:chess]
promotedPieceType = n:q
promotionRegionWhite = *8
promotionRegionBlack = *1
mandatoryPiecePromotionWhite = true
mandatoryPiecePromotionBlack = false
startFen = 4k3/1N6/8/8/8/8/1n6/4K3 w - - 0 1

[laser-piece-promotion:chess]
promotedPieceType = n:q
promotionRegionWhite = *8
mandatoryPiecePromotion = true
laserGame = true
orientedPieceTypes = n q
rotateAfterMove = true
rotationDelta = 1
laserEmitters = white@h1:1, black@a8:3
laser_n = D/D/D/D
laser_q = D/D/D/D
laser_k = D/D/D/D
castling = false
startFen = 7k/1N(0)6/8/8/8/8/8/6K1 w - - 0 1

[laser-rifle-rotation:chess]
rifleCapture = true
laserGame = true
orientedPieceTypes = r
rotateAfterMove = true
laserEmitters = white@h1:1, black@a8:3
laser_r = D/D/D/D
laser_p = D/D/D/D
laser_k = D/D/D/D
castling = false
startFen = 7k/8/8/8/8/8/p7/R5K1 w - - 0 1

INI

run_cmds() {
  local variant=${2:-promowall}
  cat <<CMDS | "${ENGINE}"
uci
setoption name VariantPath value ${TMP_VARIANT_PATH}
setoption name UCI_Variant value ${variant}
$1
quit
CMDS
}

echo "piece promotion gating regression tests started"

out=$(run_cmds "position startpos
go perft 1")
echo "${out}" | grep -q "^b7d8+,d8b7: 1$"

out=$(run_cmds "position startpos moves b7d8+,d8b7
d")
echo "${out}" | grep -q "Fen: 3+Nk3/1\*6/8/8/8/8/8/4K3 b - - 0 1"

out=$(run_cmds "position fen 4k3/8/8/8/8/8/1n6/4K3 b - - 0 1
go perft 1" "promowall-split")
echo "${out}" | grep -q "^b2d1: 1$"
! echo "${out}" | grep -q "^b2d1+,"

out=$(run_cmds "position startpos
go perft 1" "laser-piece-promotion")
echo "${out}" | grep -q "^b7d8+: 1$"
echo "${out}" | grep -q "^b7d8+,d8: 1$"

out=$(run_cmds "position startpos moves b7d8+,d8
d" "laser-piece-promotion")
echo "${out}" | grep -q "Fen: 3+N(1)3k/8/8/8/8/8/8/6K1 b - - 0 1"

out=$(run_cmds "position fen 3+N(1)3k/8/8/8/8/8/8/6K1 b - - 0 1
d" "laser-piece-promotion")
echo "${out}" | grep -q "Fen: 3+N(1)3k/8/8/8/8/8/8/6K1 b - - 0 1"

out=$(run_cmds "position startpos
go perft 1" "laser-rifle-rotation")
echo "${out}" | grep -q "^a1a2:1: 1$"
! echo "${out}" | grep -q "^a1a2:1a2:"

out=$(run_cmds "position startpos moves a1a2:1
d" "laser-rifle-rotation")
echo "${out}" | grep -q "Fen: 7k/8/8/8/8/8/8/R(1)5K1 b - - 0 1"

rm -f "${TMP_VARIANT_PATH}"
unset TMP_VARIANT_PATH

echo "piece promotion gating regression tests passed"
