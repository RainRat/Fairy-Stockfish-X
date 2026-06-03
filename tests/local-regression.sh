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

run_step "fast regression" timeout 2m bash tests/fast-regression.sh "${ENGINE}"
run_step "invalid scalar regression" timeout 30s bash tests/invalid-scalar-regression.sh "${ENGINE}"
run_step "all-vars regression" timeout 60m bash tests/allvars-regression.sh
run_step "protocol" timeout 2m bash tests/protocol.sh "${ENGINE}"
run_step "bench stdin" timeout 60s bash tests/bench-stdin.sh "${ENGINE}"
run_step "ponder stop" timeout 2m bash tests/ponder-stop.sh "${ENGINE}"
run_step "xboard regressions" timeout 2m bash tests/xboard-regressions.sh "${ENGINE}"
run_step "battleotk" timeout 2m bash tests/battleotk.sh "${ENGINE}" "${VARIANT_PATH}"
run_step "gating regressions" timeout 60s bash tests/gating-regressions.sh "${ENGINE}"
run_step "in-place transform undo" timeout 60s bash tests/in-place-transform-undo.sh "${ENGINE}"
run_step "bycatch undo parity" timeout 60s bash tests/bycatch-undo-parity.sh "${ENGINE}"
run_step "StateInfo regressions" timeout 60s bash tests/stateinfo-regressions.sh "${ENGINE}"
run_step "verbosity" timeout 60s bash tests/verbosity.sh "${ENGINE}"
run_step "state sync key" timeout 5m bash tests/state-sync-key.sh "${ENGINE}"
run_step "new variants smoke" timeout 30m bash tests/new-variants-smoke.sh "${ENGINE}" "${VARIANT_PATH}"
run_step "setup chess" timeout 2m bash tests/setup-chess.sh "${ENGINE}"
run_step "stationary castling" timeout 60s bash tests/stationary-castling.sh "${ENGINE}"
run_step "immobility illegal hoppers" timeout 60s bash tests/immobility-illegal-hoppers.sh "${ENGINE}"
run_step "pushing" timeout 60s bash tests/pushing.sh "${ENGINE}"
run_step "move morph" timeout 60s bash tests/move-morph.sh "${ENGINE}" "${VARIANT_PATH}"
run_step "same player board repetition" timeout 60s bash tests/same-player-board-repetition.sh "${ENGINE}"
run_step "petrify transfer" timeout 60s bash tests/petrify-transfer.sh "${ENGINE}"
run_step "dots and boxes" timeout 5m bash tests/dots-and-boxes.sh "${ENGINE}" "${VARIANT_PATH}" "${INCOMPLETE_VARIANT_PATH}" "${LARGE_ENGINE}" "${VLB_ENGINE}"
run_step "seega" timeout 60s bash tests/seega.sh "${ENGINE}" "${VARIANT_PATH}"
run_step "rider edge consistency" timeout 60s bash tests/rider-edge-consistency.sh "${ENGINE}" "${VARIANT_PATH}"
run_step "rose" timeout 60s bash tests/rose.sh "${ENGINE}"
run_step "bent riders" timeout 60s bash tests/bent-riders.sh "${ENGINE}" "${VARIANT_PATH}"
run_step "bent rider evasions" timeout 60s bash tests/bent-rider-evasion.sh "${ENGINE}" "${VARIANT_PATH}"
run_step "hex boards" timeout 60s bash tests/test_hex_boards.sh "${VLB_CAPABLE_ENGINE}" "${VARIANT_PATH}"
run_step "connect region 3" timeout 60s bash tests/connect-region3.sh "${ENGINE}" "${VARIANT_PATH}"
run_step "kopano" timeout 60s bash tests/kopano.sh "${ENGINE}" "${VARIANT_PATH}"
run_step "konobi" timeout 60s bash tests/konobi.sh "${ENGINE}" "${VARIANT_PATH}"
run_step "whaleshogi" timeout 60s bash tests/whaleshogi.sh "${ENGINE}"
run_step "dead pieces" timeout 60s bash tests/dead-pieces.sh "${ENGINE}"
run_step "stationary capture" timeout 60s bash tests/stationary-capture.sh "${ENGINE}"
run_step "spell freeze regressions" timeout 60s bash tests/spell-freeze-regressions.sh "${ENGINE}"
run_step "spell potion movegen" timeout 60s bash tests/spell-potion-movegen.sh "${ENGINE}"
run_step "asym rider checkers" timeout 60s bash tests/asym-rider-checkers.sh "${ENGINE}"
run_step "alfil dabbaba riders" timeout 2m bash tests/alfil-dabbaba-riders.sh "${ENGINE}"
run_step "concurrent variant magics" timeout 60s bash tests/concurrent-variant-magics.sh "${ENGINE}"
run_step "NNUE variant dimension guard" timeout 60s bash tests/nnue-variant-dimension-guard.sh "${ENGINE}"
run_step "NNUE affine regression" timeout 2m bash tests/nnue-affine-regression.sh
run_step "NNUE export failure" timeout 60s bash tests/nnue-export-failure.sh "${ENGINE}"
run_step "rootmove searchmoves" timeout 60s bash tests/rootmove-searchmoves.sh "${ENGINE}"
run_step "variant switch after perft" timeout 60s bash tests/variant-switch-after-perft.sh "${ENGINE}"
run_step "jump capture effects" timeout 60s bash tests/jump-capture-effects.sh "${ENGINE}"
run_step "edge insert" timeout 60s bash tests/edge-insert.sh "${ENGINE}"
run_step "extinction" timeout 60s bash tests/test_extinction.sh "${ENGINE}"
run_step "promotion consume in hand" timeout 60s bash tests/promotion-consume-in-hand.sh "${ENGINE}"
run_step "promotion require in hand" timeout 60s bash tests/promotion-require-in-hand.sh "${ENGINE}"
run_step "kings or lemmings" timeout 60s bash tests/kings-or-lemmings.sh "${ENGINE}" "${VARIANT_PATH}"
run_step "hindustani" timeout 60s bash tests/hindustani.sh "${ENGINE}" "${VARIANT_PATH}"
run_step "sacrifice" timeout 60s bash tests/sacrifice.sh "${ENGINE}" "${VARIANT_PATH}"
run_step "pulling" timeout 60s bash tests/pulling.sh "${ENGINE}" "${VARIANT_PATH}"
run_step "swapping" timeout 60s bash tests/swapping.sh "${ENGINE}" "${VARIANT_PATH}"
run_step "must drop by color" timeout 60s bash tests/must-drop-by-color.sh "${ENGINE}"
run_step "must capture by color" timeout 60s bash tests/must-capture-by-color.sh "${ENGINE}"
run_step "self capture color" timeout 60s bash tests/self-capture-color.sh "${ENGINE}"
run_step "self capture types" timeout 60s bash tests/self-capture-types.sh "${ENGINE}"
run_step "largeboard seirawan" timeout 60s bash tests/largeboard-seirawan.sh
run_step "VLB gale smoke" timeout 60s bash tests/vlb-gale-smoke.sh "${VLB_CAPABLE_ENGINE}" "${VARIANT_PATH}"
run_step "VLB lame riders" timeout 60s bash tests/vlb-lame-riders.sh "${VLB_CAPABLE_ENGINE}"
run_step "VLB symbol check" timeout 60s bash tests/vlb-symbol-check.sh "${VLB_CAPABLE_ENGINE}"
run_step "VLB symbol fen" timeout 60s bash tests/vlb-symbol-fen.sh "${VLB_CAPABLE_ENGINE}"
run_step "VLB symbol options" timeout 60s bash tests/vlb-symbol-options.sh "${VLB_CAPABLE_ENGINE}"
run_step "VLB symbol san" timeout 60s "${PYTHON}" tests/vlb-symbol-san.py
run_step "variant perft" timeout 30m bash tests/perft.sh all "${ENGINE}"

echo "local regression suite passed"
