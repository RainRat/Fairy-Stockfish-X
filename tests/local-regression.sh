#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
ENGINE=${1:-${ROOT_DIR}/src/stockfish-large}
VARIANTS=${VARIANT_PATH:-${ROOT_DIR}/src/variants.ini}

cd "${ROOT_DIR}"
exec tests/run.sh full "${ENGINE}"
