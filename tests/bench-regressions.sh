#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
echo "compatibility wrapper: use tests/run.sh suite search-evaluation <engine>; accepts [reference-signature] [engine] or --stdin [engine]" >&2
exec bash "${ROOT_DIR}/tests/suites/cases/search-evaluation/bench-regressions.inc" "$@"
