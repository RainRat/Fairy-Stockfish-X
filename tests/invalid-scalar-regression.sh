#!/bin/bash

set -euo pipefail

error() {
  echo "invalid scalar regression test failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
ROOT_DIR=$(cd "${SCRIPT_DIR}/.." && pwd)
ENGINE=${1:-${SCRIPT_DIR}/../src/stockfish}

cd "${ROOT_DIR}"

tmp_ini=$(mktemp)
trap 'rm -f "${tmp_ini}"' EXIT

cat > "${tmp_ini}" <<'INI'
[bad-bool-scalar:chess]
chess960 = maybe
INI

output=$("${ENGINE}" check "${tmp_ini}" 2>&1 || true)

if ! printf '%s\n' "${output}" | grep -qF "chess960 - Invalid value maybe for type bool"; then
  echo "${output}"
  exit 1
fi

if ! printf '%s\n' "${output}" | grep -qF "Variant 'bad-bool-scalar' has invalid configuration. Skipping."; then
  echo "${output}"
  exit 1
fi
