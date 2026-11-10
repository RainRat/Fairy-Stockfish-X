#!/usr/bin/env bash
set -euo pipefail

if [[ "${0##*/}" == fake-engine-* ]]; then
    case "${0##*/}" in
        fake-engine-hang-first|fake-engine-hang-second)
            if [[ "${1:-}" == bench ]]; then echo 'Nodes searched  : 42'; else exec sleep 5; fi
            ;;
        *)
            if [[ "${1:-}" == bench ]]; then
                echo 'Nodes searched  : 42'
            else
                echo 'option name UCI_Variant type combo default chess var chess var checkers'
            fi
            ;;
    esac
    exit
fi

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
mkdir -p "${ROOT_DIR}/.local/build"
WORK_DIR=$(mktemp -d "${ROOT_DIR}/.local/build/regression-discovery.XXXXXX")
trap 'rm -rf "$WORK_DIR"' EXIT

SCRIPT_PATH="${ROOT_DIR}/tests/regression-discovery.sh"
for name in fake-engine-hang-first fake-engine-hang-second fake-engine-normal; do
    ln -s "$SCRIPT_PATH" "${WORK_DIR}/${name}"
done

expect_discovery_failure() {
    local old_engine="$1" new_engine="$2" expected="$3" output status=0
    output=$(FSX_REGRESSION_PROBE_TIMEOUT=0.1 bash "${ROOT_DIR}/tests/regression.sh" \
        "$old_engine" "$new_engine" "" 2>&1) || status=$?
    if [[ $status -eq 0 || "$output" != *"variant discovery failed or timed out: ${expected}"* \
        || "$output" == *"Common variants found"* || "$output" == *" OK"* ]]; then
        echo "discovery failure check failed for ${expected}: status=${status}" >&2
        echo "$output" >&2
        return 1
    fi
}

expect_discovery_failure "${WORK_DIR}/fake-engine-hang-first" "${WORK_DIR}/fake-engine-normal" \
    "${WORK_DIR}/fake-engine-hang-first"
expect_discovery_failure "${WORK_DIR}/fake-engine-normal" "${WORK_DIR}/fake-engine-hang-second" \
    "${WORK_DIR}/fake-engine-hang-second"

output=$(FSX_REGRESSION_PROBE_TIMEOUT=1 bash "${ROOT_DIR}/tests/regression.sh" \
    "${WORK_DIR}/fake-engine-normal" "${WORK_DIR}/fake-engine-normal" "" 2>&1)
[[ "$output" == *"Common variants found: 2"* && "$output" == *"chess 42 OK"* && "$output" == *"checkers 42 OK"* ]]

# Explicit variants bypass UCI discovery, even when an engine would hang there.
output=$(FSX_REGRESSION_PROBE_TIMEOUT=1 bash "${ROOT_DIR}/tests/regression.sh" \
    "${WORK_DIR}/fake-engine-hang-first" "${WORK_DIR}/fake-engine-normal" "" chess 2>&1)
[[ "$output" == *"chess 42 OK"* ]]

echo "regression discovery checks passed"
