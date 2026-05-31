#!/bin/bash

set -euo pipefail

error() {
  echo "capture-to-hand-types regression failed on line $1" >&2
  exit 1
}
trap 'error ${LINENO}' ERR

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ENGINE="${1:-${SCRIPT_DIR}/../src/stockfish}"
source "${SCRIPT_DIR}/lib/uci.sh"

TMP_INI=$(mktemp)
trap 'rm -f "${TMP_INI}"' EXIT

cat > "${TMP_INI}" <<'INI'
[capture-to-hand-types-demo:fairy]
maxFile = h
maxRank = 7
king = k
rook = r
knight = n
lance = l
gold = g
promotedPieceType = l:g
pieceDrops = true
captureType = hand
captureToHandTypes = rl
promotionPieceTypes = -
doubleStep = false
castling = false
checking = true
INI

run_cmds() {
  run_uci "$ENGINE" "$TMP_INI" capture-to-hand-types-demo <<EOF
$1
EOF
}

variant_available() {
  local out
  out=$(printf 'uci\nquit\n' | uci_timeout "$ENGINE")
  grep -q ' var capture-to-hand-types-demo ' <<<"$out"
}

if ! variant_available; then
  echo "capture-to-hand-types-demo variant not available in this build; skipping capture-to-hand-types regression"
  exit 0
fi

# Capturing a rook should add it to hand because rook is in captureToHandTypes.
out=$(run_cmds "position fen r3k3/8/8/8/8/8/R3K3/8 w - - 0 1 moves a1a7
d")
assert_contains "$out" "Fen: R3k3/8/8/8/8/8/4K3\\[R\\] b - - 0 1"

# Capturing a knight should not add it to hand because knight is excluded.
out=$(run_cmds "position fen n3k3/8/8/8/8/8/R3K3/8 w - - 0 1 moves a1a7
d")
assert_contains "$out" "Fen: R3k3/8/8/8/8/8/4K3\\[\\] b - - 0 1"

# Capturing a promoted lance should still add a lance to hand because the filter
# applies to the transferred unpromoted piece type, not the promoted gold surface.
out=$(run_cmds "position fen +l3k3/8/8/8/8/8/R3K3/8 w - - 0 1 moves a1a7
d")
assert_contains "$out" "Fen: R3k3/8/8/8/8/8/4K3\\[L\\] b - - 0 1"
