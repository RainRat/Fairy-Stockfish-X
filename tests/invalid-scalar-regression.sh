#!/bin/bash

set -euo pipefail

error() {
  echo "invalid scalar regression test failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
ENGINE=${1:-${SCRIPT_DIR}/../src/stockfish}
source "${SCRIPT_DIR}/lib/uci.sh"

tmp_ini=$(mktemp)
trap 'rm -f "${tmp_ini}"' EXIT

cat > "${tmp_ini}" <<'INI'
[bad-bool-scalar:chess]
chess960 = maybe
INI

output=$(uci_timeout "${ENGINE}" check "${tmp_ini}" 2>&1 || true)

assert_contains_literal "$output" "chess960 - Invalid value maybe for type bool"

assert_contains_literal "$output" "Variant 'bad-bool-scalar' has invalid configuration. Skipping."
