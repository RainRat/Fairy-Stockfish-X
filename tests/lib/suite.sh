#!/usr/bin/env bash

set -euo pipefail

SUITE_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
SUITE_NAME=${1:?suite name is required}
ENGINE=${2:-${SUITE_ROOT}/src/stockfish-large}
VARIANTS=${3:-${SUITE_ROOT}/src/variants.ini}
export ROOT_DIR="${SUITE_ROOT}" ENGINE VARIANTS VARIANT_PATH="${VARIANTS}"
cd "${SUITE_ROOT}"
source "${SUITE_ROOT}/tests/lib/uci.sh"

suite_case() {
    local name="$1" timeout_value="$2" log_dir log
    shift 2
    log_dir="${SUITE_ROOT}/.local/build/test-run/cases/${SUITE_NAME}"
    log="${log_dir}/${name//\//_}.log"
    if [[ "${VERBOSE:-0}" == 1 ]]; then
        echo "== ${SUITE_NAME}/${name} =="
        if timeout "${timeout_value}" "$@"; then
            return 0
        fi
    else
        mkdir -p "${log_dir}"
        if timeout "${timeout_value}" "$@" >"${log}" 2>&1; then
            echo "ok: ${SUITE_NAME}/${name}"
            return 0
        fi
        cat "${log}"
    fi
    {
        echo "FAILED: ${SUITE_NAME}/${name}" >&2
        echo "rerun: tests/run.sh suite ${SUITE_NAME} ${ENGINE}" >&2
    }
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
    if [[ ! -f "${case_path}" ]]; then
        case_path="${SUITE_ROOT}/tests/${script}"
    fi
    if [[ ! -f "${case_path}" ]]; then
        case_path=$(find "${SUITE_ROOT}/tests/suites/cases" \( -type f -name "${script}" -o -type f -name "${script%.sh}.inc" \) -print -quit)
    fi
    [[ -n "${case_path}" && -f "${case_path}" ]] || {
        echo "missing suite case: ${script}" >&2
        return 1
    }
    suite_case "${script%.sh}" "${timeout_value}" bash "${case_path}" "$@"
}

native() {
    local group="$1"
    case "$(basename "${ENGINE}"):${FSX_ENGINE_FAMILY:-}" in
        *large*:*|*allvars*:*|*vlb*:*|*:*large|*:*very-large) ;;
        *)
            case "${group}" in occupancy|state|royal)
                echo "skip: ${SUITE_NAME}/native-${group} requires a large-board engine"
                return 0
                ;;
            esac
            ;;
    esac
    suite_case "native-${group}" 5m env FSX_REUSE_OBJECTS=1 bash "${SUITE_ROOT}/tests/native/engine-rules.sh" "${ENGINE}" "${VARIANTS}" "${group}"
}

run_config() {
    suite_case python-api-tests 3m env PYTHONPATH="${SUITE_ROOT}${PYTHONPATH:+:${PYTHONPATH}}" python3 "${SUITE_ROOT}/tests/python/test_pyffish_api.py"
    legacy parser-regressions.sh 2m "${ENGINE}"
    legacy explicit-custom-piece-replacements.sh 2m "${ENGINE}" "${VARIANTS}"
    if [[ -f "${SUITE_ROOT}/src/variants-incomplete.ini" ]]; then
        legacy incomplete-baselines.sh 2m "${ENGINE}" "${SUITE_ROOT}/src/variants-incomplete.ini"
    fi
}

run_movement() {
    native promotion
    native movement
    echo "== ${SUITE_NAME}/immobility-illegal-hopper =="
    run_immobility_illegal_hopper
    legacy movegen-regressions.sh 3m "${ENGINE}"
    legacy geometry-regressions.sh 3m "${ENGINE}" "${VARIANTS}"
    legacy rider-regressions.sh 3m "${ENGINE}" "${VARIANTS}"
    legacy fast-regression-piece-regions.sh 3m "${ENGINE}" "${VARIANTS}"
    legacy universal-hopper.sh 2m "${ENGINE}" "${VARIANTS}"
    legacy wrapping-topology.sh 2m "${ENGINE}"
    legacy test_hex_boards.sh 2m "${ENGINE}" "${VARIANTS}"
    legacy non-knight-riders.sh 2m "${ENGINE}"
    legacy separate-realms.sh 2m "${ENGINE}"
    legacy ski-sliders.sh 2m "${ENGINE}"
    legacy gadsden-toroidal.sh 2m "${ENGINE}"
    legacy rule-matrix-movement.sh 5m "${ENGINE}"
}

run_immobility_illegal_hopper() {
    load_inline_variants <<'INI'
[immobility-illegal-hopper-test:chess]
maxFile = h
maxRank = 8
pieceDrops = true
immobilityIllegal = true
king = k:W
customPiece1 = m:fpR
customPiece2 = g:W
promotedPieceType = m:g
startFen = 8/8/8/8/8/8/8/4K3[M]
INI
    local out
    out=$(run_uci "${ENGINE}" "${FSX_TMP_INI}" immobility-illegal-hopper-test <<'UCI'
position fen 8/8/8/8/8/8/8/4K3[M] w - - 0 1
go perft 1
UCI
)
    assert_contains "$out" "^M@a6: 1$"
    assert_contains "$out" "^M@e6: 1$"
    assert_not_contains "$out" "^M@a7:"
    assert_not_contains "$out" "^M@e7:"
    assert_not_contains "$out" "^M@a8:"
    assert_not_contains "$out" "^M@e8:"
}

run_royal_legality() {
    native royal
    native adjudication
    local no_kings_ini no_kings_output
    no_kings_ini=$(mktemp "${TMPDIR:-/tmp}/fsx-royal-capture-XXXXXX.ini")
    cat >"${no_kings_ini}" <<'INI'
[noroyal-capture:chess]
king = k:K
castling = false
allowChecks = true
INI
    no_kings_output=$(run_uci "${ENGINE}" "${no_kings_ini}" noroyal-capture <<'UCI'
position fen 4k3/8/8/8/4R3/8/8/4K3 w - - 0 1
go perft 1
UCI
)
    assert_contains_literal "${no_kings_output}" "e4e8: 1" "contains the royal capture"
    rm -f "${no_kings_ini}"
    legacy royal-variant-regressions.sh 3m "${ENGINE}" "${VARIANTS}"
    legacy pseudoroyal-capture-illegal.sh 2m "${ENGINE}" "${VARIANTS}"
    legacy ep-pseudoroyal-regressions.sh 2m "${ENGINE}" "${VARIANTS}"
    legacy quiet-check-special-moves.sh 5m "${ENGINE}"
    legacy gating-check-regression.sh 5m "${ENGINE}"
    legacy blast-legal-regressions.sh 2m "${ENGINE}" "${VARIANTS}"
    legacy test_extinction.sh 2m "${ENGINE}"
    legacy kings-or-lemmings.sh 2m "${ENGINE}" "${VARIANTS}"
    legacy stationary-castling.sh 2m "${ENGINE}" "${VARIANTS}"
}

run_captures_effects() {
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
    legacy rule-matrix-drops.sh 3m "${ENGINE}"
}

run_state_transitions() {
    native occupancy
    native state
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
    legacy variant-load-all.sh 10m "${ENGINE}" "${VARIANTS}"
    legacy variant-load-matrix.sh 30m "${ENGINE}" "${VARIANTS}"
    legacy variant-rules-matrix.sh 8m "${ENGINE}" "${VARIANTS}"
    legacy small-variant-rules.sh 5m "${ENGINE}" "${VARIANTS}"
    legacy connect-goals.sh 2m "${ENGINE}"
    legacy topology-smoke.sh 2m "${ENGINE}"
    legacy board-game-smoke.sh 5m "${ENGINE}" "${VARIANTS}"
    legacy variant-promotion-baselines.sh 3m "${ENGINE}" "${VARIANTS}"
    legacy gating-large-board.sh 3m "${ENGINE}" "${VARIANTS}"
    legacy royal-pawn-variants.sh 3m "${ENGINE}" "${VARIANTS}"
    legacy very-large-board-regressions.sh 10m "${ENGINE}" "${VARIANTS}"
}

run_search_evaluation() {
    legacy bench-regressions.sh 2m --stdin "${ENGINE}"
    legacy reprosearch.sh 5m "${ENGINE}"
    legacy kxk-fairy-endgames.sh 3m "${ENGINE}"
    legacy non8x8-endgames.sh 3m "${ENGINE}"
    legacy eval-geometry-regressions.sh 3m "${ENGINE}" "${VARIANTS}"
    legacy checkers-evaluation.sh 2m "${ENGINE}" "${VARIANTS}"
    legacy nnue-variant-dimension-guard.sh 2m "${ENGINE}"
    legacy nnue-affine-regression.sh 2m
    legacy nnue-export-failure.sh 2m "${ENGINE}"
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

echo "passed: ${SUITE_NAME}"
