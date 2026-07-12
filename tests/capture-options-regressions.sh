#!/bin/bash

set -euo pipefail

source "$(dirname "${BASH_SOURCE[0]}")/lib/uci.sh"

init_test_env "${1:-}"

tmp_ini=$(mktemp)
trap 'rm -f "${tmp_ini}"' EXIT

cat >"${tmp_ini}" <<'INI'
[capture-color-options:chess]
captureForbidden = p:b
captureForbiddenWhite = p:n
captureAllowedWhite = p:b
captureForbiddenBlack = p:q
captureAllowedBlack = p:b

[castling-ignore-check-options:chess]
castlingIgnoreCheck = true

[castling-check-options:chess]
castlingIgnoreCheck = false
INI

VARIANTS="${tmp_ini}"
VARIANT_PATH="${tmp_ini}"
export VARIANTS VARIANT_PATH

perft_output() {
  local variant="$1"
  local fen="$2"
  run_uci_cmds "${ENGINE}" "${VARIANT_PATH}" "${variant}" \
    "position fen ${fen}"$'\ngo perft 1'
}

# The global restriction forbids bishop captures, then the color-specific
# allow/forbid maps override it independently for each side.
white_bishop_capture=$(perft_output capture-color-options \
  '4k3/8/3b4/4P3/8/8/8/4K3 w - - 0 1')
assert_contains_literal "${white_bishop_capture}" 'e5d6: 1'

white_knight_capture=$(perft_output capture-color-options \
  '4k3/8/3n4/4P3/8/8/8/4K3 w - - 0 1')
assert_not_contains_literal "${white_knight_capture}" 'e5d6: 1'

black_bishop_capture=$(perft_output capture-color-options \
  '4K3/8/8/8/4p3/3B4/8/4k3 b - - 0 1')
assert_contains_literal "${black_bishop_capture}" 'e4d3: 1'

black_queen_capture=$(perft_output capture-color-options \
  '4K3/8/8/8/4p3/3Q4/8/4k3 b - - 0 1')
assert_not_contains_literal "${black_queen_capture}" 'e4d3: 1'

castling_fen='4k3/4r3/8/8/8/8/8/R3K2R w KQ - 0 1'
castling_allowed=$(perft_output castling-ignore-check-options "${castling_fen}")
assert_contains_literal "${castling_allowed}" 'e1g1: 1'

castling_restricted=$(perft_output castling-check-options "${castling_fen}")
assert_not_contains_literal "${castling_restricted}" 'e1g1: 1'

echo 'capture and castling option regressions passed'
