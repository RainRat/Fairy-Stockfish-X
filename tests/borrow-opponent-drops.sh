#!/usr/bin/env bash
set -euo pipefail

error() {
  echo "borrow-opponent-drops regression failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ENGINE="${1:-${SCRIPT_DIR}/../src/stockfish}"
source "${SCRIPT_DIR}/lib/uci.sh"

TMP_VARIANT_PATH=$(mktemp "${TMPDIR:-/tmp}/fsx-borrow-drops-XXXXXX")
trap 'rm -f "${TMP_VARIANT_PATH}"' EXIT

cat >"${TMP_VARIANT_PATH}" <<'INI'
[borrow-slide:fairy]
maxRank = 5
maxFile = e
pieceToCharTable = -
king = -
customPiece1 = a:-
pieceDrops = true
mustDrop = true
captureType = hand
captureToHandSide = owner
borrowOpponentDropsWhenEmpty = true
edgeInsertOnly = true
dropRegion = a* *5
edgeInsertTypes = a
edgeInsertRegion = a* *5
edgeInsertFrom = top left
pushingStrength = a:5
startFen = 5/5/5/5/5[a] w - - 0 1
INI

run_cmds() {
  run_uci "$ENGINE" "$TMP_VARIANT_PATH" borrow-slide <<EOF
$1
EOF
}

variant_available() {
  local out
  out=$(printf 'uci\nquit\n' | uci_timeout "$ENGINE")
  grep -q ' var borrow-slide ' <<<"$out"
}

if ! variant_available; then
  echo "borrow-slide variant not available in this build; skipping borrow-opponent-drops regression"
  exit 0
fi

out=$(run_cmds "position startpos
go perft 1")
assert_contains "$out" "^A@a1,b1: 1$"

out=$(run_cmds "position startpos moves A@a1,b1
d")
assert_contains "$out" "Fen: 5/5/5/5/a4[] b - - 0 1"

echo "borrow-opponent-drops regression passed"
