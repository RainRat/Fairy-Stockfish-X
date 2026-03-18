#!/usr/bin/env bash
set -euo pipefail

ENGINE="${1:-src/stockfish}"

tmp_ini="$(mktemp)"
trap 'rm -f "$tmp_ini"' EXIT

cat >"$tmp_ini" <<'EOF'
[betzarifle:chess]
customPiece1 = a:R^

[betzaplain:chess]
customPiece1 = a:R
EOF

run_d() {
  local variant="$1"
  local moves="$2"
  "$ENGINE" <<EOF
setoption name VariantPath value $tmp_ini
setoption name UCI_Variant value $variant
position fen p3k3/8/8/8/8/8/8/A3K3 w - - 0 1${moves}
d
quit
EOF
}

rifle_moves="$("$ENGINE" <<EOF
setoption name VariantPath value $tmp_ini
setoption name UCI_Variant value betzarifle
position fen p3k3/8/8/8/8/8/8/A3K3 w - - 0 1
go perft 1
quit
EOF
)"
echo "${rifle_moves}" | grep -q "^a1a8: 1$"

plain_after="$(run_d "betzaplain" " moves a1a8")"
echo "${plain_after}" | grep -q "Fen: A3k3/8/8/8/8/8/8/4K3 b - - 0 1"

rifle_after="$(run_d "betzarifle" " moves a1a8")"
echo "${rifle_after}" | grep -q "Fen: 4k3/8/8/8/8/8/8/A3K3 b - - 0 1"

echo "betza rifle notation passed"
