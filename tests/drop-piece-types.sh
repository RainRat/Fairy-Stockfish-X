#!/bin/bash

set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ENGINE="${1:-${SCRIPT_DIR}/../src/stockfish}"
source "${SCRIPT_DIR}/lib/uci.sh"

tmp_ini=$(mktemp)
trap 'rm -f "$tmp_ini"' EXIT

cat > "$tmp_ini" <<'INI'
[nana-drop-forms:chess]
maxRank = 3
maxFile = c
pieceDrops = true
customPiece1 = a:W
customPiece2 = b:F
customPiece3 = c:D
customPiece4 = d:N
dropPieceTypes = a:abcd;
dropRegionWhite = a1 b1 c1 a2 c2 a3 b3 c3
dropRegionBlack = a1 b1 c1 a2 c2 a3 b3 c3
startFen = 3/3/3[KkA] w - - 0 1
INI

out=$(run_uci "$ENGINE" "$tmp_ini" nana-drop-forms <<'EOF'
position startpos
go perft 1
EOF
)

assert_contains "$out" '^A@a1: 1$'
assert_contains "$out" '^B@a1: 1$'
assert_contains "$out" '^C@a1: 1$'
assert_contains "$out" '^D@a1: 1$'

if grep -q '@b2:' <<<"$out"; then
  echo "center square should remain excluded for all drop forms" >&2
  exit 1
fi

if grep -q '^E@' <<<"$out"; then
  echo "unexpected unconfigured drop form generated" >&2
  exit 1
fi
