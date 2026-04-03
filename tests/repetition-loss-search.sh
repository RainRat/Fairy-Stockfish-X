#!/usr/bin/env bash
set -euo pipefail

ENGINE="${1:-src/stockfish}"

tmp_ini="$(mktemp)"
trap 'rm -f "$tmp_ini"' EXIT

cat >"$tmp_ini" <<'EOF'
[rep2:chess]
king = k:W
queen = q
checking = false
nFoldRule = 2
nFoldValue = loss
startFen = 7k/5Kq1/8/8/8/8/8/8 w - - 0 1
EOF

# After the shuttle f7f6, h8h7, f6f7, black can either complete the loop with
# h7h8 and hand white a claimable repetition-loss result, or play a non-losing
# queen move. Root search should avoid the repetition-losing move.
out="$("$ENGINE" <<EOF
uci
setoption name VariantPath value $tmp_ini
setoption name UCI_Variant value rep2
position startpos moves f7f6 h8h7 f6f7
go depth 3
quit
EOF
)"
! echo "${out}" | grep -q "^bestmove h7h8$"

# Control: the move is still legal and searchable when forced.
forced="$("$ENGINE" <<EOF
uci
setoption name VariantPath value $tmp_ini
setoption name UCI_Variant value rep2
position startpos moves f7f6 h8h7 f6f7
go depth 2 searchmoves h7h8
quit
EOF
)"
echo "${forced}" | grep -q "^bestmove h7h8$"

echo "repetition-loss search regression passed"
