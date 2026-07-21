#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
CXX=${CXX:-${COMPILER:-g++}}
SUITE_DIR="${ROOT_DIR}/tests/suites"
VARIANTS=${VARIANTS:-${ROOT_DIR}/src/variants.ini}
RUN_DIR="${ROOT_DIR}/.local/build/test-run"

declare -A SUITE_TIMEOUT=(
  [config]=300 [movement]=900 [royal-legality]=900 [captures-effects]=600
  [promotion-drops]=600 [state-transitions]=900 [notation-protocol]=300
  [variants-smoke]=2400 [search-evaluation]=900 [spells]=300
)
declare -A SUITE_FAMILY=(
  [config]=large [movement]=large [royal-legality]=large [captures-effects]=large
  [promotion-drops]=large [state-transitions]=large [notation-protocol]=large
  [variants-smoke]=large [search-evaluation]=large [spells]=large
)
declare -A SUITE_PREREQS=(
  [config]=engine,python [movement]=engine,objects [royal-legality]=engine,objects
  [captures-effects]=engine [promotion-drops]=engine [state-transitions]=engine,objects,python
  [notation-protocol]=engine,expect [variants-smoke]=engine,objects [search-evaluation]=engine
  [spells]=engine,objects
)

ALL_SUITES=(config movement royal-legality captures-effects promotion-drops state-transitions notation-protocol variants-smoke search-evaluation spells)
FAST_SUITES=(config movement royal-legality captures-effects promotion-drops state-transitions notation-protocol variants-smoke spells)

default_engine() {
    if [[ -x "${ROOT_DIR}/src/stockfish-allvars" ]]; then
        echo "${ROOT_DIR}/src/stockfish-allvars"
    elif [[ -x "${ROOT_DIR}/src/stockfish-large" ]]; then
        echo "${ROOT_DIR}/src/stockfish-large"
    else
        echo "${ROOT_DIR}/src/stockfish"
    fi
}

normalize_engine() {
    local engine="$1"
    if [[ "$engine" != /* ]]; then
        engine="$(cd "$(dirname "$engine")" && pwd)/$(basename "$engine")"
    fi
    echo "$engine"
}

engine_family() {
    if [[ -n "${FSX_ENGINE_FAMILY:-}" ]]; then
        echo "${FSX_ENGINE_FAMILY}"
        return
    fi
    local base
    base=$(basename "$1")
    case "$base" in
        *vlb*) echo very-large ;;
        *large*|*allvars*) echo large ;;
        *) echo normal ;;
    esac
}

family_rank() {
    case "$1" in normal) echo 0 ;; large) echo 1 ;; very-large) echo 2 ;; esac
}

check_engine() {
    local suite="$1" engine="$2" expected actual
    [[ -x "$engine" ]] || { echo "missing executable: $engine" >&2; return 1; }
    expected=${SUITE_FAMILY[$suite]}
    actual=$(engine_family "$engine")
    if (( $(family_rank "$actual") < $(family_rank "$expected") )) && [[ "${FSX_ALLOW_SMALL_BOARD:-0}" != 1 ]]; then
        if [[ "$expected" == large ]]; then
            echo "${suite} requires a large-board all-variant engine; got ${engine} (${actual})." >&2
            echo "build with: tests/build.sh ARCH=x86-64-modern largeboards=yes all=yes EXE=stockfish-allvars" >&2
        else
            echo "${suite} requires a ${expected}-board engine; got ${engine} (${actual})" >&2
        fi
        return 1
    fi
}

check_prerequisites() {
    local suite="$1" prereq
    IFS=',' read -ra prereqs <<<"${SUITE_PREREQS[$suite]}"
    for prereq in "${prereqs[@]}"; do
        case "$prereq" in
            python) command -v python3 >/dev/null || { echo "${suite} requires python3" >&2; return 1; } ;;
            expect) command -v expect >/dev/null || { echo "${suite} requires expect" >&2; return 1; } ;;
        esac
    done
}

print_list() {
    printf '%-22s %-7s %-8s %-18s %s\n' suite timeout board prerequisites profiles
    for suite in "${ALL_SUITES[@]}"; do
        profile=full
        [[ " ${FAST_SUITES[*]} " == *" ${suite} "* ]] && profile=fast,full
        printf '%-22s %-7s %-8s %-18s %s\n' "$suite" "${SUITE_TIMEOUT[$suite]}s" "${SUITE_FAMILY[$suite]}" "${SUITE_PREREQS[$suite]}" "$profile"
    done
}

run_one() {
    local suite="$1" engine="$2" variants="$3" log_dir="${4:-}" log=""
    check_engine "$suite" "$engine"
    check_prerequisites "$suite"
    if [[ -n "$log_dir" ]]; then
        log="${log_dir}/${suite}.log"
        if timeout "${SUITE_TIMEOUT[$suite]}s" bash "${SUITE_DIR}/${suite}.sh" "$engine" "$variants" >"$log" 2>&1; then
            echo "ok: ${suite} (log: ${log})"
        else
            echo "FAILED: ${suite}"
            cat "$log"
            echo "rerun: tests/run.sh suite ${suite} ${engine}" >&2
            return 1
        fi
    else
        echo "== ${suite} =="
        if ! timeout "${SUITE_TIMEOUT[$suite]}s" bash "${SUITE_DIR}/${suite}.sh" "$engine" "$variants"; then
            echo "FAILED: ${suite}" >&2
            echo "rerun: tests/run.sh suite ${suite} ${engine}" >&2
            return 1
        fi
    fi
}

run_suite_list() {
    local engine="$1" variants="$2" suite log_dir="${3:-}"
    for suite in "${SUITES_TO_RUN[@]}"; do
        run_one "$suite" "$engine" "$variants" "$log_dir"
    done
}

prepare_python() {
    local suite needs_python=0 log="${RUN_DIR}/python-build.log"
    for suite in "${SUITES_TO_RUN[@]}"; do
        [[ "${SUITE_PREREQS[$suite]}" == *python* ]] && needs_python=1
    done
    (( needs_python )) || return 0
    mkdir -p "${ROOT_DIR}/.local/build/pyffish" "${RUN_DIR}"

    local pyffish_so=""
    shopt -s nullglob
    local pyffish_candidates=("${ROOT_DIR}"/pyffish*.so)
    shopt -u nullglob
    if (( ${#pyffish_candidates[@]} > 0 )); then
        pyffish_so="${pyffish_candidates[0]}"
    fi

    if [[ -n "${pyffish_so}" ]] && [[ "${ROOT_DIR}/setup.py" -ot "${pyffish_so}" ]]; then
        if ! find "${ROOT_DIR}/src" -type f \( -name '*.cpp' -o -name '*.h' \) -newer "${pyffish_so}" -print -quit | grep -q .; then
            return 0
        fi
    fi


    run_build() {
        if [[ "${VERBOSE:-0}" == 1 ]]; then
            (cd "${ROOT_DIR}" && python3 setup.py build_ext --inplace --build-temp "${ROOT_DIR}/.local/build/pyffish")
        elif (cd "${ROOT_DIR}" && python3 setup.py build_ext --inplace --build-temp "${ROOT_DIR}/.local/build/pyffish") >"${log}" 2>&1; then
            echo "ok: python extension"
        else
            echo "FAILED: python extension" >&2
            cat "${log}"
            return 1
        fi
    }

    local status=0
    if command -v flock >/dev/null 2>&1; then
        (
            flock -x 9 && run_build
        ) 9>"${ROOT_DIR}/.local/build/pyffish.lock" || status=1
    else
        run_build || status=1
    fi
    return $status
}


prepare_shared_objects() {
    local engine="$1"
    local needs_objects=0 suite log="${RUN_DIR}/shared-objects.log"
    for suite in "${SUITES_TO_RUN[@]}"; do
        [[ "${SUITE_PREREQS[$suite]}" == *objects* ]] && needs_objects=1
    done
    (( needs_objects )) || return 0
    mkdir -p "${RUN_DIR}"
    source "${ROOT_DIR}/tests/lib/harness-build.sh"
    fsx_harness_init "${engine}" "${ROOT_DIR}"
    if [[ "${VERBOSE:-0}" == 1 ]]; then
        fsx_harness_prepare_objects_cached "${RUN_DIR}/objects" "shared test objects" "${JOBS:-2}"
    elif fsx_harness_prepare_objects_cached "${RUN_DIR}/objects" "shared test objects" "${JOBS:-2}" >"${log}" 2>&1; then
        echo "ok: shared test objects"
    else
        echo "FAILED: shared test objects" >&2
        cat "${log}"
        return 1
    fi
    export FSX_REUSE_OBJECTS=1
}

run_fast_parallel() {
    local engine="$1" variants="$2" profile="${3:-fast}" suite pid status=0
    if [[ "${VERBOSE:-0}" == 1 ]]; then
        run_suite_list "$engine" "$variants"
        echo "${profile} profile passed"
        return 0
    fi
    mkdir -p "$RUN_DIR"
    local log_dir
    log_dir=$(mktemp -d "${RUN_DIR}/fast-XXXXXX")
    declare -A pids=()
    for suite in "${SUITES_TO_RUN[@]}"; do
        run_one "$suite" "$engine" "$variants" "$log_dir" &
        pids[$suite]=$!
    done
    for suite in "${SUITES_TO_RUN[@]}"; do
        pid=${pids[$suite]}
        if ! wait "$pid"; then status=1; fi
    done
    if (( status != 0 )); then
        echo "${profile} profile failed; logs: ${log_dir}" >&2
        return 1
    fi
    echo "${profile} profile passed"
}

usage() {
    echo "usage: tests/run.sh list | fast [engine] | full [engine] | suite <suite...> [engine]" >&2
    exit 2
}

command=${1:-}
case "$command" in
    list)
        print_list
        ;;
    fast|full)
        engine=$(normalize_engine "${2:-$(default_engine)}")
        [[ -x "$engine" ]] || { echo "missing executable: $engine" >&2; exit 1; }
        [[ "$command" == fast ]] && SUITES_TO_RUN=("${FAST_SUITES[@]}") || SUITES_TO_RUN=("${ALL_SUITES[@]}")
        prepare_python
        export CXX
        prepare_shared_objects "$engine"
        if [[ "$command" == full ]]; then
            run_fast_parallel "$engine" "$VARIANTS" "$command"
        else
            run_fast_parallel "$engine" "$VARIANTS" "$command"
        fi
        ;;
    suite)
        shift 1
        [[ $# -gt 0 ]] || usage
        engine=$(default_engine)
        if [[ -x "${!#}" ]]; then
            engine=${!#}
            args=("$@")
            unset 'args[${#args[@]}-1]'
        else
            args=("$@")
        fi
        SUITES_TO_RUN=("${args[@]}")
        for suite in "${SUITES_TO_RUN[@]}"; do
            [[ -n "${SUITE_TIMEOUT[$suite]:-}" ]] || { echo "unknown suite: $suite" >&2; exit 2; }
        done
        prepare_python
        export CXX
        prepare_shared_objects "$engine"
        engine=$(normalize_engine "$engine")
        if [[ "${VERBOSE:-0}" == 1 ]]; then
            run_suite_list "$engine" "$VARIANTS"
        else
            mkdir -p "$RUN_DIR"
            suite_log_dir=$(mktemp -d "${RUN_DIR}/suite-XXXXXX")
            run_suite_list "$engine" "$VARIANTS" "$suite_log_dir"
        fi
        ;;
    *) usage ;;
esac
