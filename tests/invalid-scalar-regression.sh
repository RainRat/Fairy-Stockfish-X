#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "invalid scalar regression"

load_inline_variants <<'INI'
[bad-bool-scalar:chess]
chess960 = maybe
INI

output=$("${ENGINE}" check "${FSX_TMP_INI}" 2>&1 || true)

assert_contains_literal "$output" "chess960 - Invalid value maybe for type bool"
assert_contains_literal "$output" "Variant 'bad-bool-scalar' has invalid configuration. Skipping."
