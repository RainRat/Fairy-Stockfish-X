#!/bin/bash

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
ENGINE=${1:-src/stockfish}
PYTHON=${PYTHON:-python3}

run_step() {
  local label="$1"
  shift
  echo "== ${label} =="
  /usr/bin/time -f "elapsed %es" "$@"
}

cd "${ROOT_DIR}"

if ! printf 'uci\nquit\n' | "${ENGINE}" | grep -q ' var duck'; then
  echo "note: ${ENGINE} does not expose 'duck' in UCI_Variant (likely non-all build); all-only alias coverage is skipped." >&2
fi

run_step "protocol" timeout 90s bash tests/protocol.sh "${ENGINE}"
run_step "movegen regressions" timeout 90s bash tests/movegen-regressions.sh "${ENGINE}"
run_step "wrapping topology" timeout 90s bash tests/wrapping-topology.sh "${ENGINE}"
run_step "unorthodox interactions" timeout 90s bash tests/unorthodox-interactions.sh "${ENGINE}"
run_step "in-place transform undo" timeout 60s bash tests/in-place-transform-undo.sh "${ENGINE}"
run_step "piece-specific step regions" timeout 60s bash tests/piece-specific-step-regions.sh "${ENGINE}"
run_step "variant switch after perft" timeout 60s bash tests/variant-switch-after-perft.sh "${ENGINE}"
run_step "custom en passant passed squares" timeout 60s bash tests/custom-en-passant-passed-squares.sh "${ENGINE}"
run_step "crazyhouse multi pawn promo" timeout 60s bash tests/crazyhouse-multi-pawn-promo.sh "${ENGINE}"
run_step "blast legal regressions" timeout 60s bash tests/blast-legal-regressions.sh "${ENGINE}"
run_step "pseudoroyal blast immune" timeout 60s bash tests/pseudoroyal-blast-immune.sh "${ENGINE}"
run_step "universal hopper" timeout 90s bash tests/universal-hopper.sh "${ENGINE}"
run_step "gating check regressions" timeout 60s bash tests/gating-check-regression.sh "${ENGINE}"
run_step "binding regression" timeout 60s "${PYTHON}" tests/test_binding_regression.py
run_step "royal capture no kings" timeout 60s "${PYTHON}" tests/test_royal_capture_no_kings.py
if [[ -n "${UPSTREAM_ENGINE:-}" ]]; then
  run_step "upstream movecount baseline" timeout 60s "${PYTHON}" tests/upstream_movecount_baseline.py "${ENGINE}" "${UPSTREAM_ENGINE}"
else
  run_step "upstream movecount baseline" timeout 60s "${PYTHON}" tests/upstream_movecount_baseline.py "${ENGINE}"
fi
run_step "python unit tests" timeout 180s "${PYTHON}" test.py

echo "fast regression suite passed"
