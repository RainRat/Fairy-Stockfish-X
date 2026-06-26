#!/bin/bash

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
export ROOT_DIR
ENGINE=${1:-src/stockfish}
PYTHON=${PYTHON:-python3}
JOBS=${JOBS:-2}
export JOBS
VARIANT_PATH="${VARIANT_PATH:-${ROOT_DIR}/src/variants.ini}"
VARIANTS="${VARIANTS:-${VARIANT_PATH}}"
export VARIANT_PATH VARIANTS
PYFFISH_BUILD_DIR="${ROOT_DIR}/.local/build/pyffish"
PYFFISH_SIG_FILE="${PYFFISH_BUILD_DIR}/fast-regression.sig"

ENGINE_RUN_DIR="${ROOT_DIR}/.local/build/fast-regression-engine"
mkdir -p "${ENGINE_RUN_DIR}"
DEFAULT_ENGINE_COPY="${ENGINE_RUN_DIR}/$(basename "${ENGINE}")"
cp -f "${ENGINE}" "${DEFAULT_ENGINE_COPY}"
chmod +x "${DEFAULT_ENGINE_COPY}"
ENGINE="${DEFAULT_ENGINE_COPY}"
export ENGINE

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
  local i pid label log
  for i in "${!PIDS[@]}"; do
    pid="${PIDS[$i]}"
    label="${LABELS[$i]}"
    log="${LOGS[$i]}"

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

dispatch_test() {
  local label="$1"
  shift
  run_step_bg "$label" "$@"
}

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

dispatch_test "piece-specific step regions" bash tests/fast-regression-piece-regions.sh "${ENGINE}" "${VARIANT_PATH}"
dispatch_test "fast variant regressions" timeout 5m bash tests/fast-variant-regressions.sh "${ENGINE}" "${VARIANT_PATH}"
dispatch_test "fast rules regression" bash tests/fast-regression-rules.sh "${ENGINE}" "${VARIANT_PATH}"
dispatch_test "binding regression" timeout 60s "${PYTHON}" tests/test_binding_regression.py
dispatch_test "royal capture no kings" timeout 60s "${PYTHON}" tests/test_royal_capture_no_kings.py
dispatch_test "touched search regressions" timeout 2m bash tests/touched-search-regressions.sh "${ENGINE}" "${VARIANT_PATH}"
dispatch_test "setup chess" timeout 2m bash tests/setup-chess.sh "${ENGINE}" "${VARIANT_PATH}"
dispatch_test "xboard regressions" timeout 2m bash tests/xboard-regressions.sh "${ENGINE}" "${VARIANT_PATH}"
dispatch_test "hex board regressions" timeout 2m bash tests/test_hex_boards.sh "${ENGINE}" "${VARIANT_PATH}"
if [[ -n "${UPSTREAM_ENGINE:-}" ]]; then
  dispatch_test "upstream movecount baseline" timeout 60s "${PYTHON}" tests/upstream_movecount_baseline.py "${ENGINE}" "${UPSTREAM_ENGINE}"
else
  dispatch_test "upstream movecount baseline" timeout 60s "${PYTHON}" tests/upstream_movecount_baseline.py "${ENGINE}"
fi
dispatch_test "python unit tests" timeout 180s "${PYTHON}" test.py

wait_all

run_step "quiet-check special moves" timeout 5m bash tests/quiet-check-special-moves.sh "${ENGINE}"
run_step "gating check regressions" timeout 5m bash tests/gating-check-regression.sh "${ENGINE}"

echo "fast regression suite passed"
