#!/bin/bash

set -euo pipefail

error() {
  echo "multimove rule50 test failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-./stockfish}
VARIANT_PATH=${2:-src/variants.ini}

extract_fen() {
  sed -n 's/^Fen: //p' | tail -n1
}

position_dump() {
  local variant="$1"
  local pos_cmd="$2"
  cat <<CMDS | "$ENGINE"
uci
setoption name VariantPath value ${VARIANT_PATH}
setoption name UCI_Variant value ${variant}
${pos_cmd}
d
quit
CMDS
}

assert_fen() {
  local variant="$1"
  local pos_cmd="$2"
  local expected="$3"
  local fen
  fen=$(position_dump "${variant}" "${pos_cmd}" | extract_fen)
  if [[ "${fen}" != "${expected}" ]]; then
    echo "Unexpected FEN for ${variant}"
    echo "position: ${pos_cmd}"
    echo "expected: ${expected}"
    echo "actual:   ${fen}"
    return 1
  fi
}

echo "multimove rule50 tests started"

# A mandatory Marseillais pass should not advance the halfmove clock.
assert_fen \
  "marseillais" \
  "position startpos moves e2e4 e8e8" \
  "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2"

# Likewise after a non-pawn opening move, only the actual move should count.
assert_fen \
  "marseillais" \
  "position startpos moves g1f3 e8e8" \
  "rnbqkbnr/pppppppp/8/8/8/5N2/PPPPPPPP/RNBQKB1R w KQkq - 1 2"

echo "multimove rule50 tests passed"
