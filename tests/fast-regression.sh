#!/bin/bash

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
export ROOT_DIR
ENGINE=${1:-src/stockfish}
PYTHON=${PYTHON:-python3}
JOBS=${JOBS:-2}
VERBOSE=${VERBOSE:-0}
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

hash_object_tree() {
  local obj
  shopt -s nullglob
  for obj in "${ROOT_DIR}"/src/*.o; do
    printf '%s %s\n' "${obj##*/}" "$(hash_file "${obj}")"
  done
  shopt -u nullglob
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

  run_step_quiet "pyffish extension" timeout 10m "${PYTHON}" setup.py build_ext --inplace --build-temp "${PYFFISH_BUILD_DIR}"
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

run_step_quiet() {
  local label="$1"
  shift
  local safe_label="${label//[^a-zA-Z0-9_]/_}"
  local log_file="${TEMP_LOG_DIR}/${safe_label}.log"
  local elapsed

  if (
    echo "== ${label} =="
    /usr/bin/time -f "elapsed %es" "$@"
  ) > "${log_file}" 2>&1; then
    if [[ "${VERBOSE}" == "1" ]]; then
      cat "${log_file}"
    else
      elapsed=$(awk '/^elapsed / {value=$2} END {print value}' "${log_file}")
      printf 'ok: %s%s\n' "${label}" "${elapsed:+ (${elapsed})}"
    fi
  else
    echo "FAILED: ${label}"
    cat "${log_file}"
    return 1
  fi
}

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
  local i pid label log elapsed
  for i in "${!PIDS[@]}"; do
    pid="${PIDS[$i]}"
    label="${LABELS[$i]}"
    log="${LOGS[$i]}"

    if wait "$pid"; then
      if [[ "${VERBOSE}" == "1" ]]; then
        cat "$log"
      else
        elapsed=$(awk '/^elapsed / {value=$2} END {print value}' "$log")
        printf 'ok: %s%s\n' "$label" "${elapsed:+ (${elapsed})}"
      fi
    else
      exit_code=1
      echo "FAILED: ${label}"
      cat "$log"
    fi
  done

  PIDS=()
  LABELS=()
  LOGS=()

  if [[ $exit_code -ne 0 ]]; then
    echo "fast regression suite failed"
    exit 1
  fi
}

prepare_harness_objects() {
  local cache_dir="${ROOT_DIR}/.local/build/regression-harness-objects"
  local desired_sig object_sig cxx_version
  mkdir -p "${cache_dir}"
  cxx_version=$("${CXX:-g++}" --version | head -n1)
  desired_sig=$(printf '%s|%s|%s|%s|%s\n' \
    "${ENGINE_BASENAME}" "${cxx_version}" "$(hash_file "${ROOT_DIR}/src/Makefile")" \
    "$(hash_file "${ROOT_DIR}/tests/fast-regression.sh")" "$(hash_source_tree)")
  object_sig=$(hash_object_tree | hash_file /dev/stdin)

  if [[ -f "${cache_dir}/desired.sig" && -f "${cache_dir}/objects.sig" ]] \
      && [[ "$(<"${cache_dir}/desired.sig")" == "${desired_sig}" ]] \
      && [[ "$(<"${cache_dir}/objects.sig")" == "${object_sig}" ]]; then
    echo "ok: prepare regression harness objects (cached)"
    return
  fi

  case "${ENGINE_BASENAME}" in
    stockfish)
      run_step_quiet "prepare regression harness objects" timeout 30m bash -lc \
        'cd src && make -s EXE=stockfish objclean && make -s -j"${JOBS}" build ARCH=x86-64 EXE=stockfish'
      ;;
    stockfish-allvars*)
      run_step_quiet "prepare regression harness objects" timeout 30m bash -lc \
        'cd src && make -s EXE=stockfish-allvars objclean && make -s -j"${JOBS}" build ARCH=x86-64 largeboards=yes all=yes nnue=yes EXE=stockfish-allvars'
      ;;
    stockfish-large*)
      run_step_quiet "prepare regression harness objects" timeout 30m bash -lc \
        'cd src && make -s EXE=stockfish-large objclean && make -s -j"${JOBS}" build ARCH=x86-64 largeboards=yes all=yes EXE=stockfish-large'
      ;;
    stockfish-vlb*)
      run_step_quiet "prepare regression harness objects" timeout 30m bash -lc \
        'cd src && make -s EXE=stockfish-vlb objclean && make -s -j"${JOBS}" build ARCH=x86-64 largeboards=yes verylargeboards=yes all=yes nnue=yes EXE=stockfish-vlb'
      ;;
    *)
      return
      ;;
  esac

  object_sig=$(hash_object_tree | hash_file /dev/stdin)
  printf '%s\n' "${desired_sig}" > "${cache_dir}/desired.sig"
  printf '%s\n' "${object_sig}" > "${cache_dir}/objects.sig"
}

dispatch_test() {
  local label="$1"
  shift
  run_step_bg "$label" "$@"
}

ENGINE_BASENAME=$(basename "${ENGINE}")

setup_parallel
ensure_pyffish_extension
export PYTHONPATH="${ROOT_DIR}${PYTHONPATH:+:${PYTHONPATH}}"

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

prepare_harness_objects
dispatch_test "quiet-check special moves" timeout 5m env FSX_REUSE_OBJECTS=1 bash tests/quiet-check-special-moves.sh "${ENGINE}"
dispatch_test "gating check regressions" timeout 5m env FSX_REUSE_OBJECTS=1 bash tests/gating-check-regression.sh "${ENGINE}"
wait_all

echo "fast regression suite passed"
