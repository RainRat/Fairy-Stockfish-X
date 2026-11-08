#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
ENGINE=${1:-${ROOT_DIR}/src/stockfish-allvars}
if [[ $# -eq 0 && -x "${ROOT_DIR}/src/stockfish-allvars" ]]; then
    echo "fast regression: using ${ROOT_DIR}/src/stockfish-allvars for large-board suite coverage"
    ENGINE="${ROOT_DIR}/src/stockfish-allvars"
fi
exec "${ROOT_DIR}/tests/run.sh" fast "${ENGINE}"
