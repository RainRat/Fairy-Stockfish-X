#!/bin/bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

ENGINE="${1:-}"
if [[ -z "$ENGINE" ]]; then
  if [[ -x "${ROOT_DIR}/src/stockfish-vlb" ]]; then
    ENGINE="${ROOT_DIR}/src/stockfish-vlb"
  else
    ENGINE="${ROOT_DIR}/stockfish-vlb"
  fi
fi

TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT

VARIANT_FILE="$TMPDIR/vlb-lame-riders.ini"
cat > "$VARIANT_FILE" <<'VAR'
[vlb-lame-clear:fairy]
maxFile = p
maxRank = 16
customPiece1 = a:n{path:mid}D
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 12A2k/16/16/16/16/16/16/16/16/16/16/16/16/16/16/15K w - - 0 1

[vlb-lame-blocked:fairy]
maxFile = p
maxRank = 16
customPiece1 = a:n{path:mid}D
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 12A2k/12p3/16/16/16/16/16/16/16/16/16/16/16/16/16/15K w - - 0 1
VAR

run_variant() {
  local variant="$1"
  run_uci "$ENGINE" "$VARIANT_FILE" "$variant" <<'UCI' 2>&1
position startpos
go perft 1
UCI
}

out=$(run_variant vlb-lame-clear)
if grep -q "exceeds build board limits" <<<"$out"; then
  echo "skip: VLB lame rider regression requires a very-large-board capable engine"
  exit 0
fi
if grep -q "No such variant" <<<"$out"; then
  echo "skip: VLB lame rider variant unavailable in this binary"
  exit 0
fi

assert_contains "$out" "info string variant vlb-lame-clear files 16 ranks 16"
assert_contains "$out" "^m16m14: 1$"
assert_contains "$out" "^m16k16: 1$"
assert_contains "$out" "^m16o16: 1$"
assert_nodes "$out" 6

out=$(run_variant vlb-lame-blocked)
assert_contains "$out" "info string variant vlb-lame-blocked files 16 ranks 16"
assert_not_contains "$out" "^m16m14:"
assert_contains "$out" "^m16k16: 1$"
assert_contains "$out" "^m16o16: 1$"
assert_nodes "$out" 5

echo "VLB lame rider regression passed"
