#!/usr/bin/env bash
# The per-thread pawn and material caches are keyed by placement and piece
# count keys, neither of which identifies the variant. Two variants that
# produce the same key can therefore disagree about the cached payload, so a
# variant switch must drop both caches. Each case primes the cache in `first`
# and reads it back in `second`, requiring the row a fresh `second`-only
# session produces.

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/../../../../tests/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "eval variant cache reset"

row_of() {
  grep -i "$2" <<<"$1" | tail -n 1 | tr -s ' '
}

# Prime the caches in `first_variant`, switch to `second_variant`, and compare
# the selected eval row against a session that never loads `first_variant`.
expect_switch_match() {
  local first_variant="$1" first_fen="$2" second_variant="$3" second_fen="$4"
  local label="$5" pattern="$6"
  local sequential fresh switched reference

  sequential=$(cat <<CMDS | "${ENGINE}"
uci
setoption name VariantPath value ${VARIANTS}
setoption name UCI_Variant value ${first_variant}
setoption name Use NNUE value false
position fen ${first_fen}
eval
setoption name UCI_Variant value ${second_variant}
position fen ${second_fen}
eval
quit
CMDS
)
  fresh=$(cat <<CMDS | "${ENGINE}"
uci
setoption name VariantPath value ${VARIANTS}
setoption name UCI_Variant value ${second_variant}
setoption name Use NNUE value false
position fen ${second_fen}
eval
quit
CMDS
)

  switched=$(row_of "${sequential}" "${pattern}")
  reference=$(row_of "${fresh}" "${pattern}")

  if [[ -z "${switched}" || -z "${reference}" ]]; then
    echo "variant cache reset test produced no '${pattern}' row (${label})" >&2
    exit 1
  fi
  if [[ "${switched}" != "${reference}" ]]; then
    echo "stale eval cache across variant switch (${label}):" \
         "'${switched}' vs fresh '${reference}'" >&2
    exit 1
  fi
}

# Material cache: material keys cover piece counts, so {K,K,R} collides between
# an 8x8 and a 6x6 board, but the cached material density divides by the board
# area. A reused entry reports the density of the board it was built for.
if probe_variant_available "${ENGINE}" losalamos "${VARIANTS}"; then
  expect_switch_match chess '4k3/8/8/8/8/8/8/R3K3 w - - 0 1' \
                      losalamos '5k/6/6/6/6/R4K w - - 0 1' \
                      "material-density" "final evaluation"
else
  fsx_test_note_skip "losalamos is unavailable in ${ENGINE##*/}"
fi

# Pawn cache: the pawn key covers placement only, while mandatory captures
# scale the isolated/doubled penalties. The coffeehouse entry must not be
# served from the chess entry for the same pawn skeleton.
if probe_variant_available "${ENGINE}" coffeehouse "${VARIANTS}"; then
  expect_switch_match chess '4k3/8/8/8/8/8/P2P4/4K3 w - - 0 1' \
                      coffeehouse '4k3/8/8/8/8/8/P2P4/4K3 w - - 0 1' \
                      "pawn-must-capture" "| *pawns |"
else
  fsx_test_note_skip "coffeehouse is unavailable in ${ENGINE##*/}"
fi

echo "eval variant cache reset tests passed"
