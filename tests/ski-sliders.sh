#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/../src"

tmp_ini="$(mktemp)"
trap 'rm -f "$tmp_ini"' EXIT

cat >"$tmp_ini" <<'EOF'
[skirook:chess]
customPiece1 = s:jR
pieceToCharTable = PNBRQ.............SKpnbrq.............sk
startFen = 8/8/8/8/3S4/8/8/4K2k w - - 0 1

[skibishop:chess]
customPiece1 = a:jB
pieceToCharTable = PNBRQ.............AKpnbrq.............ak
startFen = 8/8/8/8/3A4/8/8/4K2k w - - 0 1
EOF

perft_nodes() {
  local variant="$1"
  local fen="$2"
  ./stockfish <<EOF | awk '/Nodes searched:/{print $3}'
uci
setoption name VariantPath value $tmp_ini
setoption name UCI_Variant value $variant
isready
position fen $fen
go perft 1
quit
EOF
}

expect_nodes() {
  local variant="$1"
  local fen="$2"
  local expected="$3"
  local got
  got="$(perft_nodes "$variant" "$fen")"
  if [[ "$got" != "$expected" ]]; then
    echo "FAIL: $variant fen='$fen' expected $expected got $got"
    exit 1
  fi
}

# jR skips adjacent orthogonal squares, so an adjacent blocker on e4 is ignored.
expect_nodes skirook "8/8/8/8/3S4/8/8/4K2k w - - 0 1" 15
expect_nodes skirook "8/8/8/8/3Sp3/8/8/4K2k w - - 0 1" 15
# Landing blocker on f4 reduces right-ray continuation.
expect_nodes skirook "8/8/8/8/3S1p2/8/8/4K2k w - - 0 1" 13

# jB skips adjacent diagonals.
expect_nodes skibishop "8/8/8/8/3A4/8/8/4K2k w - - 0 1" 14
expect_nodes skibishop "8/8/8/8/3A4/4p3/8/4K2k w - - 0 1" 12
expect_nodes skibishop "8/8/8/8/3A4/5p2/8/4K2k w - - 0 1" 13

echo "ski-sliders test OK"
