#!/bin/bash

set -euo pipefail

error() {
  echo "piece promotion gating regression failed on line $1"
  [[ -n "${TMP_VARIANT_PATH:-}" ]] && rm -f "${TMP_VARIANT_PATH}"
  exit 1
}
trap 'error ${LINENO}' ERR

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
ENGINE="${1:-${REPO_ROOT}/src/stockfish}"

TMP_VARIANT_PATH=$(mktemp /tmp/fsx-promowall-XXXXXX.ini)
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
echo "${out}" | grep -q "^b7d8+,b7: 1$"

out=$(run_cmds "position startpos moves b7d8+,b7
d")
echo "${out}" | grep -q "Fen: 3+Nk3/1\\*6/8/8/8/8/8/4K3 b - - 0 1"

out=$(run_cmds "position fen 4k3/8/8/8/8/8/1n6/4K3 b - - 0 1
go perft 1" "promowall-split")
echo "${out}" | grep -q "^b2d1: 1$"
! echo "${out}" | grep -q "^b2d1+,"

rm -f "${TMP_VARIANT_PATH}"
unset TMP_VARIANT_PATH

echo "piece promotion gating regression tests passed"