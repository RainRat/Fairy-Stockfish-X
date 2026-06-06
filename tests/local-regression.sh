#!/bin/bash

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
export ROOT_DIR
ENGINE=${1:-src/stockfish}
PYTHON=${PYTHON:-python3}
VARIANT_PATH=${VARIANT_PATH:-src/variants.ini}
VARIANTS="${VARIANTS:-${VARIANT_PATH}}"
export VARIANT_PATH VARIANTS
INCOMPLETE_VARIANT_PATH=${INCOMPLETE_VARIANT_PATH:-src/variants-incomplete.ini}
VLB_ENGINE=${VLB_ENGINE:-src/stockfish-vlb}
LARGE_ENGINE=${LARGE_ENGINE:-src/stockfish-large}
MINI_ENGINE=${MINI_ENGINE:-src/stockfish-allvars}

run_step() {
  local label="$1"
  shift
  echo "== ${label} =="
  /usr/bin/time -f "elapsed %es" "$@"
}

rm -rf "${ROOT_DIR}/.local/build"
ENGINE_RUN_DIR="${ROOT_DIR}/.local/build/local-regression-engine"
mkdir -p "${ENGINE_RUN_DIR}"
DEFAULT_ENGINE_COPY="${ENGINE_RUN_DIR}/stockfish"
cp -f "${ENGINE}" "${DEFAULT_ENGINE_COPY}"
chmod +x "${DEFAULT_ENGINE_COPY}"
ENGINE="${DEFAULT_ENGINE_COPY}"
export ENGINE

cd "${ROOT_DIR}"

run_step "fast regression" timeout 30m bash tests/fast-regression.sh "${ENGINE}"
if [[ -x "${ROOT_DIR}/${MINI_ENGINE}" || -x "${MINI_ENGINE}" ]]; then
  run_step "mini variant regressions" timeout 2m bash tests/mini-variant-regressions.sh "${MINI_ENGINE}" "${VARIANT_PATH}"
fi
run_step "geometry regressions" timeout 2m bash tests/geometry-regressions.sh "${MINI_ENGINE}" "${VARIANT_PATH}"
run_step "rider regressions" timeout 2m bash tests/rider-regressions.sh "${MINI_ENGINE}" "${VARIANT_PATH}"
run_step "capture promotion regressions" timeout 2m bash tests/capture-promotion-regressions.sh "${ENGINE}" "${VARIANT_PATH}"
run_step "misc engine regressions" timeout 2m bash tests/misc-engine-regressions.sh "${ENGINE}" "${VARIANT_PATH}"
run_step "special regressions" timeout 5m bash tests/special-regressions.sh "${ENGINE}" "${VARIANT_PATH}"
run_step "local inline regression" timeout 5m bash tests/local-regression-inline.sh "${ENGINE}" "${VARIANT_PATH}"
run_step "movegen regressions" timeout 90s bash tests/movegen-regressions.sh "${ENGINE}"
run_step "royal variant regressions" timeout 2m bash tests/royal-variant-regressions.sh "${ENGINE}" "${VARIANT_PATH}"
run_step "drop regressions" timeout 2m bash tests/drop-regressions.sh "${ENGINE}"
run_step "fairy notation regressions" timeout 2m bash tests/fairy-notation-regressions.sh "${ENGINE}"
run_step "wrapping topology" timeout 90s bash tests/wrapping-topology.sh "${ENGINE}"
run_step "unorthodox interactions" timeout 90s bash tests/unorthodox-interactions.sh "${ENGINE}"
run_step "universal hopper" timeout 90s bash tests/universal-hopper.sh "${ENGINE}"
run_step "all-vars regression" timeout 60m bash tests/allvars-regression.sh
if [[ -x "${LARGE_ENGINE}" ]]; then
  ENGINE="${LARGE_ENGINE}"
  export ENGINE
fi
run_step "protocol" timeout 2m bash tests/protocol.sh "${ENGINE}"
run_step "bench stdin" timeout 60s bash tests/bench-regressions.sh --stdin "${ENGINE}"
run_step "xboard regressions" timeout 2m bash tests/xboard-regressions.sh "${ENGINE}"
run_step "gating regressions" timeout 60s bash tests/gating-regressions.sh "${ENGINE}"
run_step "in-place transform undo" timeout 60s bash tests/in-place-transform-undo.sh "${ENGINE}"
run_step "bycatch undo parity" timeout 60s bash tests/bycatch-undo-parity.sh "${ENGINE}"
run_step "StateInfo regressions" timeout 3m bash tests/stateinfo-regressions.sh "${ENGINE}"
run_step "verbosity" timeout 60s bash tests/verbosity.sh "${ENGINE}"
run_step "state sync key" timeout 5m bash tests/state-sync-key.sh "${ENGINE}"
run_step "new variants smoke" timeout 30m bash tests/new-variants-smoke.sh "${ENGINE}" "${VARIANT_PATH}"
run_step "setup chess" timeout 2m bash tests/setup-chess.sh "${ENGINE}"
run_step "stationary castling" timeout 60s bash tests/stationary-castling.sh "${ENGINE}"
run_step "move morph" timeout 60s bash tests/move-morph.sh "${ENGINE}" "${VARIANT_PATH}"
run_step "petrify transfer" timeout 60s bash tests/petrify-transfer.sh "${ENGINE}"
run_step "dots and boxes" timeout 5m bash tests/dots-and-boxes.sh "${ENGINE}" "${VARIANT_PATH}" "${INCOMPLETE_VARIANT_PATH}" "${LARGE_ENGINE}" "${VLB_ENGINE}"
run_step "rose" timeout 60s bash tests/rose.sh "${ENGINE}"
run_step "hex boards" timeout 60s bash tests/test_hex_boards.sh "${ENGINE}" "${VARIANT_PATH}"
run_step "spell freeze regressions" timeout 60s bash tests/spell-freeze-regressions.sh "${ENGINE}"
run_step "spell potion movegen" timeout 60s bash tests/spell-potion-movegen.sh "${ENGINE}"
run_step "asym rider checkers" timeout 60s bash tests/asym-rider-checkers.sh "${ENGINE}"
run_step "alfil dabbaba riders" timeout 2m bash tests/alfil-dabbaba-riders.sh "${ENGINE}"
run_step "concurrent variant magics" timeout 60s bash tests/concurrent-variant-magics.sh "${ENGINE}"
run_step "NNUE variant dimension guard" timeout 60s bash tests/nnue-variant-dimension-guard.sh "${ENGINE}"
run_step "NNUE affine regression" timeout 2m bash tests/nnue-affine-regression.sh
run_step "NNUE export failure" timeout 60s bash tests/nnue-export-failure.sh "${ENGINE}"
run_step "rootmove searchmoves" timeout 60s bash tests/rootmove-searchmoves.sh "${ENGINE}"
run_step "jump capture effects" timeout 60s bash tests/jump-capture-effects.sh "${ENGINE}"
run_step "edge insert" timeout 60s bash tests/edge-insert.sh "${ENGINE}"
run_step "extinction" timeout 60s bash tests/test_extinction.sh "${ENGINE}"
run_step "kings or lemmings" timeout 60s bash tests/kings-or-lemmings.sh "${ENGINE}" "${VARIANT_PATH}"
run_step "pulling" timeout 60s bash tests/pulling.sh "${ENGINE}" "${VARIANT_PATH}"
run_step "swapping" timeout 60s bash tests/swapping.sh "${ENGINE}" "${VARIANT_PATH}"
run_step "largeboard seirawan" timeout 60s bash tests/largeboard-seirawan.sh
if [[ -x "${VLB_ENGINE}" || -x "${ROOT_DIR}/${VLB_ENGINE}" ]]; then
  run_step "VLB regressions" timeout 60s bash tests/vlb-regressions.sh "${VLB_ENGINE}" "${VARIANT_PATH}"
fi
run_step "VLB symbol san" timeout 60s "${PYTHON}" tests/vlb-symbol-san.py
run_step "variant perft" timeout 30m bash tests/perft.sh all "${ENGINE}"

echo "local regression suite passed"
