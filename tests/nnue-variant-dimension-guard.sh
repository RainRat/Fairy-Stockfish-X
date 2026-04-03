#!/usr/bin/env bash
set -euo pipefail

ENGINE="${1:-src/stockfish}"

tmp_ini="$(mktemp)"
trap 'rm -f "$tmp_ini"' EXIT

cat >"$tmp_ini" <<'EOF'
[nnguard:crazyhouse]
customPiece1 = a:W
customPiece2 = c:F
customPiece3 = d:N
customPiece4 = e:B
customPiece5 = f:R
customPiece6 = g:Q
customPiece7 = h:K
customPiece8 = i:A
EOF

out="$("$ENGINE" <<EOF
uci
setoption name VariantPath value $tmp_ini
setoption name UCI_Variant value nnguard
setoption name EvalFile value nnguard.nnue
position startpos
go depth 1
quit
EOF
)"

echo "${out}" | grep -q "info string NNUE disabled for variant nnguard"
echo "${out}" | grep -q "info string classical evaluation enabled"
! echo "${out}" | grep -q "The option is set to true, but the network file"

echo "nnue variant dimension guard passed"
