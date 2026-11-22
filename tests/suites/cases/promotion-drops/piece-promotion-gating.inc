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

[shogi-forced-gate:chess]
gating = true
gatingFromHand = false
gatingPieceAfter = r:n p:n
promotedPieceType = r:q p:g
promotionRegionWhite = *8
promotionRegionBlack = *1
promotionPieceTypes = -
gold = g
castling = false

[promo-gate-mismatch:chess]
gating = true
gatingFromHand = false
gatingPieceAfter = p:n
promotionPieceTypes = q
castling = false

[shogi-seirawan-gate:chess]
seirawanGating = true
gating = true
promotedPieceType = r:q
promotionRegionWhite = *2
promotionPieceTypes = -
castling = false
startFen = 4k3/8/8/8/8/8/8/R3K3[N] w A - 0 1

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

# PIECE_PROMOTION derives its promoted type from the moving piece and does not
# collide with gating_type, so both non-pawn and pawn forced gating still fire.
out=$(run_cmds "position fen 4k3/R7/8/8/8/8/8/4K3 w - - 0 1 moves a7a8+
d" "shogi-forced-gate")
echo "${out}" | grep -q "Fen: +R3k3/N7/8/8/8/8/8/4K3 b - - 0 1"

out=$(run_cmds "position fen 4k3/P7/8/8/8/8/8/4K3 w - - 0 1 moves a7a8+
d" "shogi-forced-gate")
echo "${out}" | grep -q "Fen: +P3k3/N7/8/8/8/8/8/4K3 b - - 0 1"

# Mismatched PROMOTION + forced gating still promotes to the PROMOTION result.
out=$(run_cmds "position fen 4k3/P7/8/8/8/8/8/4K3 w - - 0 1 moves a7a8q
d" "promo-gate-mismatch")
echo "${out}" | grep -q "Fen: Q3k3/8/8/8/8/8/8/4K3 b - - 0 1"

# Seirawan gating emits both plain and gated PIECE_PROMOTION moves.
out=$(run_cmds "position startpos
go perft 1" "shogi-seirawan-gate")
[[ $(echo "${out}" | grep -c '^a1a2+: 1$') -eq 2 ]]

rm -f "${TMP_VARIANT_PATH}"
unset TMP_VARIANT_PATH

echo "piece promotion gating regression tests passed"
