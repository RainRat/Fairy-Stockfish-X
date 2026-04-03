#!/usr/bin/env bash
set -euo pipefail

ENGINE="${1:-src/stockfish}"

tmp_ini="$(mktemp)"
trap 'rm -f "$tmp_ini"' EXIT

cat >"$tmp_ini" <<'EOF'
[extstal:chess]
extinctionValue = loss
extinctionPieceTypes = p
stalemateValue = loss
EOF

out="$("$ENGINE" <<EOF
uci
setoption name VariantPath value $tmp_ini
setoption name UCI_Variant value extstal
setoption name Verbosity value 2
position fen 7k/5Q2/7K/8/8/8/8/8 b - - 0 1
go depth 1
quit
EOF
)"

echo "${out}" | grep -q "info string adjudication reason game_end"
echo "${out}" | grep -q "^bestmove (none)$"

echo "extinction-stalemate precedence passed"
