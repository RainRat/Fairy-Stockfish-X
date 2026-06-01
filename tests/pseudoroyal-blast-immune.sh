#!/bin/bash

set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

error() {
  echo "pseudoroyal-blast-immune test failed on line $1"
  [[ -n "${TMP:-}" ]] && rm -f "${TMP}"
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE="$(default_engine "${1:-}")"

run_cmds() {
  local variant_path="$1"
  local variant="$2"
  local cmds="$3"
  run_uci "${ENGINE}" "${variant_path}" "${variant}" <<< "${cmds}"
}

echo "pseudoroyal-blast-immune tests started"

TMP=$(mktemp "${TMPDIR:-/tmp}/fsx-immune-blast-XXXXXX")
cat >"${TMP}" <<'INI'
[immune-blast-test:atomic]
blastImmuneTypes = k
pseudoRoyalCaptureIllegal = true
INI

# Test 1: Black king has no legal moves, but is touching the White King.
# White King is checked by a rook, wait... no, the test was: Black king has NO legal moves.
# FEN: k1R5/K7/8/8/8/8/8/8 b - - 0 1
# White Rook on c8 checks Black King on a8.
# Black King cannot capture White King on a7 because of pseudoRoyalCaptureIllegal.
# Since both kings are blastImmune (via blastImmuneTypes=k), the stalemate_value function
# correctly detects that the White King is immune to the potential blast from Black King,
# and thus the "touching kings are immune to check from each other" rule (which only applies
# if they are NOT blastImmune) doesn't falsely trigger. So this should evaluate as checkmate.
out=$(run_cmds "${TMP}" "immune-blast-test" "position fen k1R5/K7/8/8/8/8/8/8 b - - 0 1
go depth 1")
assert_contains_literal "${out}" "score mate 0" "report mate score"

rm -f "${TMP}"

echo "pseudoroyal-blast-immune tests passed"
