#!/bin/bash

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
ENGINE=${1:-src/stockfish}
PYTHON=${PYTHON:-python3}
JOBS=${JOBS:-2}
export JOBS
PYFFISH_BUILD_DIR="${ROOT_DIR}/.local/build/pyffish"
PYFFISH_SIG_FILE="${PYFFISH_BUILD_DIR}/fast-regression.sig"

run_step() {
  local label="$1"
  shift
  echo "== ${label} =="
  /usr/bin/time -f "elapsed %es" "$@"
}

TEMP_LOG_DIR=""
PIDS=()
LABELS=()
LOGS=()

setup_parallel() {
  TEMP_LOG_DIR=$(mktemp -d "${TMPDIR:-/tmp}/fast-regression-logs-XXXXXX")
}

cleanup_parallel() {
  if [[ -n "${TEMP_LOG_DIR:-}" && -d "${TEMP_LOG_DIR}" ]]; then
    rm -rf "${TEMP_LOG_DIR}"
  fi
}

trap cleanup_parallel EXIT

run_step_bg() {
  local label="$1"
  shift
  local safe_label="${label//[^a-zA-Z0-9_]/_}"
  local log_file="${TEMP_LOG_DIR}/${safe_label}.log"

  (
    echo "== ${label} =="
    /usr/bin/time -f "elapsed %es" "$@"
  ) > "${log_file}" 2>&1 &

  PIDS+=($!)
  LABELS+=("${label}")
  LOGS+=("${log_file}")
}

wait_all() {
  local exit_code=0
  for i in "${!PIDS[@]}"; do
    local pid="${PIDS[$i]}"
    local label="${LABELS[$i]}"
    local log="${LOGS[$i]}"

    if ! wait "$pid"; then
      exit_code=1
    fi
    cat "$log"
  done

  if [[ $exit_code -ne 0 ]]; then
    echo "fast regression suite failed"
    exit 1
  fi
}

hash_file() {
  local path="$1"
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$path" | awk '{print $1}'
  elif command -v shasum >/dev/null 2>&1; then
    shasum -a 256 "$path" | awk '{print $1}'
  else
    wc -c < "$path" | awk '{print $1}'
  fi
}

hash_source_tree() {
  if command -v sha256sum >/dev/null 2>&1; then
    find src -type f \( -name '*.cpp' -o -name '*.h' \) -print0 \
      | sort -z \
      | xargs -0 sha256sum \
      | sha256sum \
      | awk '{print $1}'
  elif command -v shasum >/dev/null 2>&1; then
    find src -type f \( -name '*.cpp' -o -name '*.h' \) -print0 \
      | sort -z \
      | xargs -0 shasum -a 256 \
      | shasum -a 256 \
      | awk '{print $1}'
  else
    find src -type f \( -name '*.cpp' -o -name '*.h' \) -print0 \
      | sort -z \
      | xargs -0 wc -c \
      | awk '{sum += $1} END {print sum}'
  fi
}

ensure_pyffish_extension() {
  local setup_hash source_hash py_version cxx_version current_sig cached_sig pyffish_so=""

  mkdir -p "${PYFFISH_BUILD_DIR}"

  shopt -s nullglob
  local pyffish_candidates=("${ROOT_DIR}"/pyffish*.so)
  shopt -u nullglob
  if (( ${#pyffish_candidates[@]} > 0 )); then
    pyffish_so="${pyffish_candidates[0]}"
  fi

  setup_hash=$(hash_file "${ROOT_DIR}/setup.py")
  source_hash=$(hash_source_tree)
  py_version=$("${PYTHON}" -V 2>&1)
  cxx_version=$("${CXX:-g++}" --version | head -n1)
  current_sig=$(printf '%s|%s|%s|%s\n' "${setup_hash}" "${source_hash}" "${py_version}" "${cxx_version}")

  if [[ -f "${PYFFISH_SIG_FILE}" ]] && [[ -n "${pyffish_so}" ]]; then
    cached_sig=$(<"${PYFFISH_SIG_FILE}")
    if [[ "${cached_sig}" == "${current_sig}" ]]; then
      return
    fi
  fi

  if [[ -n "${pyffish_so}" ]] && [[ "${ROOT_DIR}/setup.py" -ot "${pyffish_so}" ]]; then
    if ! find src -type f \( -name '*.cpp' -o -name '*.h' \) -newer "${pyffish_so}" -print -quit | grep -q .; then
      printf '%s\n' "${current_sig}" > "${PYFFISH_SIG_FILE}"
      return
    fi
  fi

  run_step "pyffish extension" timeout 10m "${PYTHON}" setup.py build_ext --inplace --build-temp "${PYFFISH_BUILD_DIR}"
  printf '%s\n' "${current_sig}" > "${PYFFISH_SIG_FILE}"
}

cd "${ROOT_DIR}"

if ! printf 'uci\nquit\n' | "${ENGINE}" | grep -q ' var duck'; then
  echo "note: ${ENGINE} does not expose 'duck' in UCI_Variant (likely non-all build); all-only alias coverage is skipped." >&2
fi

ENGINE_BASENAME=$(basename "${ENGINE}")
case "${ENGINE_BASENAME}" in
  stockfish-large*)
    run_step "prep largeboard objects" timeout 30m bash -lc 'cd src && make -s EXE=stockfish-large objclean && make -s -j"${JOBS}" build ARCH=x86-64 largeboards=yes all=yes EXE=stockfish-large'
    ;;
  stockfish-vlb*)
    run_step "prep very-large-board objects" timeout 30m bash -lc 'cd src && make -s EXE=stockfish-vlb objclean && make -s -j"${JOBS}" build ARCH=x86-64 largeboards=yes verylargeboards=yes all=yes EXE=stockfish-vlb'
    ;;
esac

ensure_pyffish_extension
export PYTHONPATH="${ROOT_DIR}${PYTHONPATH:+:${PYTHONPATH}}"

setup_parallel

run_step_bg "janggi regressions" timeout 60s bash tests/janggi-regression.sh "${ENGINE}"
run_step_bg "piece-specific step regions" timeout 60s bash tests/piece-specific-step-regions.sh "${ENGINE}"
run_step_bg "pawn-like custom non-step" timeout 60s bash tests/pawnlike-custom-nonstep.sh "${ENGINE}"
run_step_bg "custom en passant passed squares" timeout 60s bash tests/custom-en-passant-passed-squares.sh "${ENGINE}"
run_step_bg "standard piece value phase" timeout 60s bash tests/standard-piece-value-phase.sh "${ENGINE}"
run_step_bg "flip regressions" timeout 60s bash tests/flip-regressions.sh "${ENGINE}"
run_step_bg "changing-color locality" timeout 60s bash tests/changing-color-locality.sh "${ENGINE}"
run_step_bg "potion check regressions" timeout 60s bash tests/potion-check-regressions.sh "${ENGINE}"
run_step_bg "passive blast" timeout 60s bash tests/passive-blast.sh "${ENGINE}"
run_step_bg "crazyhouse multi pawn promo" timeout 60s bash tests/crazyhouse-multi-pawn-promo.sh "${ENGINE}"
run_step_bg "pousse counting" timeout 60s bash tests/pousse-counting.sh "${ENGINE}"
run_step_bg "repetition loss search" timeout 60s bash tests/repetition-loss-search.sh "${ENGINE}"
run_step_bg "quiet-check special moves" timeout 5m bash tests/quiet-check-special-moves.sh "${ENGINE}"
run_step_bg "gating check regressions" timeout 60s bash tests/gating-check-regression.sh "${ENGINE}"
run_step_bg "binding regression" timeout 60s "${PYTHON}" tests/test_binding_regression.py
run_step_bg "royal capture no kings" timeout 60s "${PYTHON}" tests/test_royal_capture_no_kings.py
run_step_bg "potion custom" timeout 60s bash tests/potion-custom.sh "${ENGINE}"
if [[ -n "${UPSTREAM_ENGINE:-}" ]]; then
  run_step_bg "upstream movecount baseline" timeout 60s "${PYTHON}" tests/upstream_movecount_baseline.py "${ENGINE}" "${UPSTREAM_ENGINE}"
else
  run_step_bg "upstream movecount baseline" timeout 60s "${PYTHON}" tests/upstream_movecount_baseline.py "${ENGINE}"
fi
run_step_bg "python unit tests" timeout 180s "${PYTHON}" test.py

wait_all

echo "fast regression suite passed"
