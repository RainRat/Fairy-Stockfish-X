#!/bin/bash

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
ENGINE=${1:-src/stockfish}
PYTHON=${PYTHON:-python3}
VARIANT_PATH=${VARIANT_PATH:-src/variants.ini}
INCOMPLETE_VARIANT_PATH=${INCOMPLETE_VARIANT_PATH:-src/variants-incomplete.ini}
VLB_ENGINE=${VLB_ENGINE:-src/stockfish-vlb}
LARGE_ENGINE=${LARGE_ENGINE:-src/stockfish-large}

run_step() {
  local label="$1"
  shift
  echo "== ${label} =="
  /usr/bin/time -f "elapsed %es" "$@"
}

VLB_CAPABLE_ENGINE="${ENGINE}"
if [[ -x "${VLB_ENGINE}" ]]; then
  VLB_CAPABLE_ENGINE="${VLB_ENGINE}"
fi

cd "${ROOT_DIR}"

run_step "fast regression" timeout 30m bash tests/fast-regression.sh "${ENGINE}"
run_step "all-vars regression" timeout 60m bash tests/allvars-regression.sh
run_step "protocol" timeout 2m bash tests/protocol.sh "${ENGINE}"
run_step "new variants smoke" timeout 30m bash tests/new-variants-smoke.sh "${ENGINE}" "${VARIANT_PATH}"
run_step "incomplete baselines" timeout 10m bash tests/incomplete-baselines.sh "${ENGINE}" "${INCOMPLETE_VARIANT_PATH}"
run_step "rider edge consistency" timeout 60s bash tests/rider-edge-consistency.sh "${ENGINE}" "${VARIANT_PATH}"
run_step "rose" timeout 60s bash tests/rose.sh "${ENGINE}"
run_step "bent riders" timeout 60s bash tests/bent-riders.sh "${ENGINE}" "${VARIANT_PATH}"
run_step "bent rider evasions" timeout 60s bash tests/bent-rider-evasion.sh "${ENGINE}" "${VARIANT_PATH}"
run_step "hex board display" timeout 60s bash tests/hex-board-display.sh "${ENGINE}"
run_step "hex piece movement" timeout 60s bash tests/hex-piece-movement.sh "${ENGINE}" "${VARIANT_PATH}"
run_step "connect region 3" timeout 60s bash tests/connect-region3.sh "${ENGINE}" "${VARIANT_PATH}"
run_step "kopano" timeout 60s bash tests/kopano.sh "${ENGINE}" "${VARIANT_PATH}"
run_step "konobi" timeout 60s bash tests/konobi.sh "${ENGINE}" "${VARIANT_PATH}"
run_step "whaleshogi" timeout 60s bash tests/whaleshogi.sh "${ENGINE}"
run_step "Betza modifiers" timeout 60s bash tests/betza-modifiers.sh "${ENGINE}"
run_step "asym rider checkers" timeout 60s bash tests/asym-rider-checkers.sh "${ENGINE}"
run_step "common fairy aliases" timeout 60s bash tests/common-fairy-aliases.sh "${ENGINE}"
run_step "alfil dabbaba riders" timeout 60s bash tests/alfil-dabbaba-riders.sh "${ENGINE}"
run_step "concurrent variant magics" timeout 60s bash tests/concurrent-variant-magics.sh "${ENGINE}"
run_step "NNUE variant dimension guard" timeout 60s bash tests/nnue-variant-dimension-guard.sh "${ENGINE}"
run_step "NNUE affine regression" timeout 2m bash tests/nnue-affine-regression.sh
run_step "NNUE export failure" timeout 60s bash tests/nnue-export-failure.sh "${ENGINE}"
run_step "rootmove searchmoves" timeout 60s bash tests/rootmove-searchmoves.sh "${ENGINE}"
run_step "jump capture effects" timeout 60s bash tests/jump-capture-effects.sh "${ENGINE}"
run_step "largeboard seirawan" timeout 60s bash tests/largeboard-seirawan.sh
run_step "dots and boxes" timeout 10m bash tests/dots-and-boxes.sh "${ENGINE}" "${VARIANT_PATH}" "${INCOMPLETE_VARIANT_PATH}" "${LARGE_ENGINE}" "${VLB_ENGINE}"
run_step "hex chess variants" timeout 10m bash tests/hex-chess-variants.sh "${VLB_CAPABLE_ENGINE}" "${VARIANT_PATH}"
run_step "hex connection variants" timeout 10m bash tests/hex-connection-variants.sh "${VLB_CAPABLE_ENGINE}" "${VARIANT_PATH}"
run_step "VLB gale smoke" timeout 60s bash tests/vlb-gale-smoke.sh "${VLB_CAPABLE_ENGINE}" "${VARIANT_PATH}"
run_step "VLB symbol check" timeout 60s bash tests/vlb-symbol-check.sh "${VLB_CAPABLE_ENGINE}"
run_step "VLB symbol fen" timeout 60s bash tests/vlb-symbol-fen.sh "${VLB_CAPABLE_ENGINE}"
run_step "VLB symbol options" timeout 60s bash tests/vlb-symbol-options.sh "${VLB_CAPABLE_ENGINE}"
run_step "VLB symbol san" timeout 60s "${PYTHON}" tests/vlb-symbol-san.py
run_step "variant perft" timeout 30m bash tests/perft.sh all "${ENGINE}"

echo "local regression suite passed"
