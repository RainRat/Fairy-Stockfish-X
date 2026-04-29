#!/bin/bash

set -euo pipefail

error() {
  echo "pseudoroyal-blast-immune test failed on line $1"
  [[ -n "${TMP:-}" ]] && rm -f "${TMP}"
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-./stockfish}

run_cmds() {
  local variant_path="$1"
  local variant="$2"
  local cmds="$3"
  cat <<CMDS | "${ENGINE}"
uci
setoption name VariantPath value ${variant_path}
setoption name UCI_Variant value ${variant}
${cmds}
quit
CMDS
}

echo "pseudoroyal-blast-immune tests started"

TMP=$(mktemp /tmp/fsx-immune-blast-XXXXXX.ini)
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
echo "${out}" | grep -q "score mate 0" || error ${LINENO}

rm -f "${TMP}"

echo "pseudoroyal-blast-immune tests passed"
