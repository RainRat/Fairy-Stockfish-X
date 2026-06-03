#!/bin/bash

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "explicit custom piece replacement regression"

run_perft() {
  local variant="$1"
  local fen="$2"

  run_uci "$ENGINE" "$VARIANTS" "$variant" <<EOF
position fen ${fen}
go perft 1
EOF
}

variant_available() {
  local variant="$1"

  probe_variant_available "$ENGINE" "$variant" "$VARIANTS"
}

echo "explicit custom piece replacement regression tests started"

# British bishop = BmW on 10x10.
if variant_available "british-chess"; then
  out=$(run_perft "british-chess" "4q5/10/10/10/4B5/10/10/10/10/4Q5 w - - 0 1")
  echo "${out}" | grep -q "^e6e7: 1$"
  echo "${out}" | grep -q "^e6f7: 1$"
  echo "${out}" | grep -q "^e6j6: 1$" && exit 1
fi

# Chaturanga al-Adli bishop = D on 8x8.
out=$(run_perft "chaturanga-al-adli" "rnbfk1nr/pppppppp/8/8/3B4/8/PPPPPPPP/RN1FK1NR w - - 0 1")
echo "${out}" | grep -q "^d4d6: 1$"
echo "${out}" | grep -q "^d4f4: 1$"
echo "${out}" | grep -q "^d4f6: 1$" && exit 1

echo "explicit custom piece replacement regression tests passed"
