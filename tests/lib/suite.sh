#!/usr/bin/env bash

set -euo pipefail

SUITE_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
SUITE_NAME=${1:?suite name is required}
ENGINE=${2:-${SUITE_ROOT}/src/stockfish-large}
VARIANTS=${3:-${SUITE_ROOT}/src/variants.ini}
FSX_CASE_MATCHED=0
FSX_CASES_REQUIRED=0
FSX_CASES_EXECUTED=0
FSX_CASES_SKIPPED=0
FSX_CASES_FAILED=0
export ROOT_DIR="${SUITE_ROOT}" ENGINE VARIANTS VARIANT_PATH="${VARIANTS}"
cd "${SUITE_ROOT}"
source "${SUITE_ROOT}/tests/lib/uci.sh"

if [[ "${VERBOSE:-0}" != 1 && -z "${FSX_CASE_LOG_ROOT:-}" ]]; then
    mkdir -p "${SUITE_ROOT}/.local/build/test-run"
    FSX_CASE_LOG_ROOT=$(mktemp -d "${SUITE_ROOT}/.local/build/test-run/direct-XXXXXX")/cases
    export FSX_CASE_LOG_ROOT
fi

fsx_suite_summary() {
    local status=$?
    trap - EXIT
    printf 'FSX_TEST_SUMMARY\t1\t%s\trequired=%d\texecuted=%d\tskipped=%d\tfailed=%d\n' \
        "${SUITE_NAME}" "${FSX_CASES_REQUIRED}" "${FSX_CASES_EXECUTED}" \
        "${FSX_CASES_SKIPPED}" "${FSX_CASES_FAILED}"
    exit "${status}"
}
trap fsx_suite_summary EXIT

fsx_case_event() {
    local name="$1" result="$2" duration="$3" requirement="$4" reason="${5:-}"
    reason=${reason//$'\t'/ }
    reason=${reason//$'\n'/ }
    printf 'FSX_TEST_EVENT\t1\t%s\t%s\t%s\t%s\t%s\t%s\n' \
        "${SUITE_NAME}" "${name}" "${result}" "${duration}" \
        "${requirement}" "${reason}"
}

suite_case() {
    local name="$1" timeout_value="$2" log_dir="" log="" start=$SECONDS duration status=0
    local requirement=required reason="" was_skip=0
    shift 2
    if [[ -n "${FSX_CASE_FILTER:-}" ]]; then
        [[ "${name}" == "${FSX_CASE_FILTER}" ]] || return 0
        FSX_CASE_MATCHED=1
    fi
    if [[ "${FSX_CASE_OPTIONAL:-0}" == 1 && "${FSX_ALLOW_SMALL_BOARD:-0}" == 1 ]]; then
        requirement=optional
    else
        ((FSX_CASES_REQUIRED += 1))
    fi
    if [[ "${VERBOSE:-0}" == 1 ]]; then
        echo "== ${SUITE_NAME}/${name} =="
        mkdir -p "${SUITE_ROOT}/.local/build/test-run"
        log=$(mktemp "${SUITE_ROOT}/.local/build/test-run/verbose-case-XXXXXX")
        timeout "${timeout_value}" "$@" >"${log}" 2>&1 || status=$?
        cat "${log}"
    else
        log_dir="${FSX_CASE_LOG_ROOT}/${SUITE_NAME}"
        log="${log_dir}/${name//\//_}.log"
        mkdir -p "${log_dir}"
        timeout "${timeout_value}" "$@" >"${log}" 2>&1 || status=$?
    fi
    duration=$((SECONDS - start))
    if (( status == 0 )); then
        ((FSX_CASES_EXECUTED += 1))
        fsx_case_event "${name}" pass "${duration}" "${requirement}"
        echo "ok: ${SUITE_NAME}/${name} (${duration}s)"
        [[ "${VERBOSE:-0}" != 1 ]] || rm -f "${log}"
        return 0
    fi
    if (( status == 77 )); then
        reason=$(sed -n 's/^FSX_TEST_SKIP: //p' "${log}" | sed -n '1p')
        ((FSX_CASES_SKIPPED += 1))
        if [[ "${requirement}" == optional && "${FSX_ALLOW_SMALL_BOARD:-0}" == 1 && -n "${reason}" ]]; then
            fsx_case_event "${name}" skip "${duration}" "${requirement}" "${reason}"
            echo "skip: ${SUITE_NAME}/${name} (${reason})"
            [[ "${VERBOSE:-0}" != 1 ]] || rm -f "${log}"
            return 0
        fi
        was_skip=1
        status=1
        reason="unexpected skip${reason:+: ${reason}}"
    fi
    (( was_skip )) || ((FSX_CASES_EXECUTED += 1))
    ((FSX_CASES_FAILED += 1))
    [[ "${VERBOSE:-0}" == 1 ]] || cat "${log}"
    fsx_case_event "${name}" fail "${duration}" "${requirement}" "${reason:-exit ${status}}"
    {
        echo "FAILED: ${SUITE_NAME}/${name} (${duration}s)" >&2
        printf 'rerun: tests/run.sh case %q %q %q %q\n' \
            "${SUITE_NAME}" "${name}" "${ENGINE}" "${VARIANTS}" >&2
    }
    [[ "${VERBOSE:-0}" != 1 ]] || rm -f "${log}"
    return 1
}

legacy() {
    local script="$1" timeout_value="$2" case_path fragment
    shift 2
    case_path="${SUITE_ROOT}/tests/suites/cases/${SUITE_NAME}/${script}"
    fragment="${SUITE_ROOT}/tests/suites/cases/${SUITE_NAME}/${script%.sh}.inc"
    if [[ ! -f "${case_path}" && -f "${fragment}" ]]; then
        case_path="${fragment}"
    fi
    [[ -n "${case_path}" && -f "${case_path}" ]] || {
        echo "missing suite case: ${SUITE_NAME}/${script}" >&2
        return 1
    }
    suite_case "${script%.sh}" "${timeout_value}" bash "${case_path}" "$@"
}

optional_legacy() {
    FSX_CASE_OPTIONAL=1 legacy "$@"
}

native() {
    local group="$1"
    case "$(basename "${ENGINE}"):${FSX_ENGINE_FAMILY:-}" in
        *large*:*|*allvars*:*|*vlb*:*|*:*large|*:*very-large) ;;
        *)
            case "${group}" in occupancy|state|royal)
                if [[ "${FSX_ALLOW_SMALL_BOARD:-0}" == 1 ]]; then
                    echo "SKIP-BOARDSIZE: ${SUITE_NAME}/native-${group} requires a large-board engine; skip allowed by FSX_ALLOW_SMALL_BOARD=1" >&2
                    return 0
                fi
                echo "native-${group} requires a large-board engine; got ${ENGINE}" >&2
                echo "run with a large-board engine or set FSX_ALLOW_SMALL_BOARD=1 to allow the skip" >&2
                return 1
                ;;
            esac
            ;;
    esac
    suite_case "native-${group}" 5m env FSX_REUSE_OBJECTS=1 bash "${SUITE_ROOT}/tests/native/engine-rules.sh" "${ENGINE}" "${VARIANTS}" "${group}"
}

run_config() {
    suite_case python-api-tests 3m env PYTHONPATH="${SUITE_ROOT}${PYTHONPATH:+:${PYTHONPATH}}" python3 "${SUITE_ROOT}/tests/python/test_pyffish_api.py"
    legacy parser-regressions.sh 5m "${ENGINE}"
    legacy explicit-custom-piece-replacements.sh 2m "${ENGINE}" "${VARIANTS}"
    if [[ -f "${SUITE_ROOT}/src/variants-incomplete.ini" ]]; then
        legacy incomplete-baselines.sh 2m "${ENGINE}" "${SUITE_ROOT}/src/variants-incomplete.ini"
    fi
}

run_movement() {
    native promotion
    native movement
    legacy immobility-illegal-hopper.sh 2m "${ENGINE}"
    legacy movegen-regressions.sh 3m "${ENGINE}"
    legacy geometry-regressions.sh 3m "${ENGINE}" "${VARIANTS}"
    legacy rider-regressions.sh 3m "${ENGINE}" "${VARIANTS}"
    legacy fast-regression-piece-regions.sh 3m "${ENGINE}" "${VARIANTS}"
    legacy universal-hopper.sh 5m "${ENGINE}" "${VARIANTS}"
    legacy wrapping-topology.sh 2m "${ENGINE}"
    legacy test_hex_boards.sh 2m "${ENGINE}" "${VARIANTS}"
    legacy non-knight-riders.sh 2m "${ENGINE}"
    legacy separate-realms.sh 2m "${ENGINE}"
    legacy ski-sliders.sh 2m "${ENGINE}"
    legacy gadsden-toroidal.sh 2m "${ENGINE}"
    legacy immobilizer.sh 2m "${ENGINE}" "${VARIANTS}"
    legacy rule-matrix-movement.sh 5m "${ENGINE}"
}

run_royal_legality() {
    native royal
    native adjudication
    native extinction-color
    legacy no-royal-capture.sh 2m "${ENGINE}"
    legacy royal-variant-regressions.sh 3m "${ENGINE}" "${VARIANTS}"
    legacy chu-lion-rules.sh 2m "${ENGINE}" "${VARIANTS}"
    legacy pseudoroyal-capture-illegal.sh 2m "${ENGINE}" "${VARIANTS}"
    legacy ep-pseudoroyal-regressions.sh 2m "${ENGINE}" "${VARIANTS}"
    legacy quiet-check-special-moves.sh 5m "${ENGINE}"
    legacy gating-check-regression.sh 5m "${ENGINE}"
    legacy blast-legal-regressions.sh 2m "${ENGINE}" "${VARIANTS}"
    legacy test_extinction.sh 2m "${ENGINE}"
    legacy nonroyal-draw-threshold.sh 2m "${ENGINE}"
    legacy connection-both-sides.sh 2m "${ENGINE}"
    legacy simul-priority.sh 2m "${ENGINE}"
    legacy kings-or-lemmings.sh 2m "${ENGINE}" "${VARIANTS}"
    legacy stationary-castling.sh 2m "${ENGINE}" "${VARIANTS}"
}

run_captures_effects() {
    native locust-all
    legacy capture-options-regressions.sh 2m "${ENGINE}"
    legacy blast-pattern.sh 2m "${ENGINE}" "${VARIANTS}"
    legacy jump-capture-effects.sh 2m "${ENGINE}"
    legacy rifle-chess.sh 2m "${ENGINE}"
    legacy petrify-transfer.sh 2m "${ENGINE}"
    legacy pulling.sh 2m "${ENGINE}" "${VARIANTS}"
    legacy swapping.sh 2m "${ENGINE}" "${VARIANTS}"
    legacy color-change-variants.sh 2m "${ENGINE}"
    legacy capture-interactions.sh 3m "${ENGINE}"
    legacy capture-effects-special.sh 5m "${ENGINE}" "${VARIANTS}"
    legacy capture-rule-definitions.sh 8m "${ENGINE}" "${VARIANTS}"
    legacy cross-feature-state.sh 8m "${ENGINE}" "${VARIANTS}"
    legacy mortal-chessgi.sh 2m "${ENGINE}" "${VARIANTS}"
    legacy rule-matrix-captures.sh 5m "${ENGINE}"
}

run_promotion_drops() {
    legacy capture-promotion-regressions.sh 3m "${ENGINE}" "${VARIANTS}"
    legacy drop-regressions.sh 3m "${ENGINE}"
    legacy piece-promotion-gating.sh 3m "${ENGINE}"
    legacy chained-piece-promotion.sh 2m "${ENGINE}"
    legacy promotion-require-in-prison.sh 2m "${ENGINE}"
    legacy shogi-pawn-drop-mate-split.sh 2m "${ENGINE}"
    legacy castling-promoted-piece.sh 2m "${ENGINE}"
    legacy wrapping-promotion-movegen.sh 2m "${ENGINE}"
    legacy skica.sh 2m "${ENGINE}" "${VARIANTS}"
    legacy rule-matrix-drops.sh 3m "${ENGINE}"
}

run_state_transitions() {
    native occupancy
    native state
    native composable-rules
    legacy stateinfo-regressions.sh 5m "${ENGINE}"
    legacy state-sync-key.sh 5m "${ENGINE}"
    legacy in-place-transform-undo.sh 2m "${ENGINE}"
    legacy bycatch-undo-parity.sh 2m "${ENGINE}"
    legacy clone-firstmove-split.sh 2m "${ENGINE}"
    legacy multimove-rule50.sh 2m "${ENGINE}"
    legacy concurrent-variant-magics.sh 2m "${ENGINE}"
    legacy touched-search-regressions.sh 3m "${ENGINE}" "${VARIANTS}"
    legacy piece-type-bitboard-regressions.sh 2m
}

run_notation_protocol() {
    legacy fairy-notation-regressions.sh 3m "${ENGINE}"
    legacy protocol.sh 3m "${ENGINE}"
    legacy xboard-regressions.sh 3m "${ENGINE}" "${VARIANTS}"
    legacy setup-chess.sh 3m "${ENGINE}" "${VARIANTS}"
}

run_variants_smoke() {
    native board-games
    legacy variant-load-all.sh 15m "${ENGINE}" "${VARIANTS}"
    legacy variant-load-matrix.sh 30m "${ENGINE}" "${VARIANTS}"
    legacy variant-rules-matrix.sh 8m "${ENGINE}" "${VARIANTS}"
    legacy small-variant-rules.sh 5m "${ENGINE}" "${VARIANTS}"
    legacy connect-goals.sh 2m "${ENGINE}"
    legacy topology-smoke.sh 2m "${ENGINE}"
    legacy board-game-smoke.sh 5m "${ENGINE}" "${VARIANTS}"
    legacy variant-promotion-baselines.sh 3m "${ENGINE}" "${VARIANTS}"
    legacy gating-large-board.sh 3m "${ENGINE}" "${VARIANTS}"
    legacy royal-pawn-variants.sh 3m "${ENGINE}" "${VARIANTS}"
    optional_legacy very-large-board-regressions.sh 10m "${ENGINE}" "${VARIANTS}"
}

run_search_evaluation() {
    legacy bench-regressions.sh 2m --stdin "${ENGINE}"
    legacy reprosearch.sh 5m "${ENGINE}"
    legacy kxk-fairy-endgames.sh 3m "${ENGINE}"
    legacy non8x8-endgames.sh 3m "${ENGINE}"
    legacy eval-geometry-regressions.sh 3m "${ENGINE}" "${VARIANTS}"
    legacy asymmetric-extinction-evaluation.sh 3m "${ENGINE}"
    legacy checkers-evaluation.sh 2m "${ENGINE}" "${VARIANTS}"
    legacy nnue-variant-dimension-guard.sh 2m "${ENGINE}"
    legacy nnue-affine-regression.sh 2m
    legacy nnue-export-failure.sh 2m "${ENGINE}"
    suite_case nnue-loading 2m python3 "${SUITE_ROOT}/tests/nnue-loading.py" "${ENGINE}"
    legacy engine-search-regressions.sh 15m "${ENGINE}" "${VARIANTS}"
}

run_spells() {
    legacy spell-freeze-regressions.sh 2m "${ENGINE}" "${VARIANTS}"
    legacy spell-potion-movegen.sh 3m "${ENGINE}"
}

case "${SUITE_NAME}" in
    config) run_config ;;
    movement) run_movement ;;
    royal-legality) run_royal_legality ;;
    captures-effects) run_captures_effects ;;
    promotion-drops) run_promotion_drops ;;
    state-transitions) run_state_transitions ;;
    notation-protocol) run_notation_protocol ;;
    variants-smoke) run_variants_smoke ;;
    search-evaluation) run_search_evaluation ;;
    spells) run_spells ;;
    *) echo "unknown suite: ${SUITE_NAME}" >&2; exit 2 ;;
esac

if [[ -n "${FSX_CASE_FILTER:-}" && "${FSX_CASE_MATCHED}" != 1 ]]; then
    echo "unknown suite case: ${SUITE_NAME}/${FSX_CASE_FILTER}" >&2
    exit 2
fi

if [[ -n "${FSX_CASE_FILTER:-}" ]]; then
    echo "passed: ${SUITE_NAME}/${FSX_CASE_FILTER}"
else
    echo "passed: ${SUITE_NAME}"
fi
