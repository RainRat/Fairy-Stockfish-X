#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
echo "compatibility wrapper: use tests/run.sh suite search-evaluation <engine>; accepts [reference-signature] [engine] or --stdin [engine]" >&2
ENGINE=${2:-${ROOT_DIR}/src/stockfish-allvars}
[[ "${1:-}" == --stdin ]] && ENGINE=${2:-${ROOT_DIR}/src/stockfish-allvars}
source "${ROOT_DIR}/tests/lib/direct-engine.sh"
fsx_check_direct_engine "${ROOT_DIR}" "${ENGINE}" large-allvars
exec bash "${ROOT_DIR}/tests/suites/cases/search-evaluation/bench-regressions.inc" "$@"
