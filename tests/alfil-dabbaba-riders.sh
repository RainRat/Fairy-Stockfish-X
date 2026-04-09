#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")/../src"

tmp_ini=$(mktemp)
trap 'rm -f "$tmp_ini"' EXIT

cat > "$tmp_ini" <<'INI'
[alfil-rider:chess]
customPiece1 = a:AA
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 7k/8/8/8/3A4/8/8/K7 w - - 0 1

[alfil-rider-tuple:chess]
customPiece1 = a:(2,2)(2,2)
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 7k/8/8/8/3A4/8/8/K7 w - - 0 1

[dabbaba-rider:chess]
customPiece1 = a:DD
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 7k/8/8/8/3A4/8/8/K7 w - - 0 1

[dabbaba-rider-tuple:chess]
customPiece1 = a:(2,0)2
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 7k/8/8/8/3A4/8/8/K7 w - - 0 1
INI

piece_moves() {
  local variant=$1
  printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value %s\nposition startpos\ngo perft 1\nquit\n' "$tmp_ini" "$variant" \
    | ./stockfish \
    | awk -F: '/^d4/{print $1}' \
    | sort
}

expected_alfil=$(mktemp)
expected_dabbaba=$(mktemp)
actual_alfil=$(mktemp)
actual_alfil_tuple=$(mktemp)
actual_dabbaba=$(mktemp)
actual_dabbaba_tuple=$(mktemp)
trap 'rm -f "$tmp_ini" "$expected_alfil" "$expected_dabbaba" "$actual_alfil" "$actual_alfil_tuple" "$actual_dabbaba" "$actual_dabbaba_tuple"' EXIT

cat > "$expected_alfil" <<'EOF'
d4b2
d4b6
d4f2
d4f6
d4h8
EOF

cat > "$expected_dabbaba" <<'EOF'
d4b4
d4d2
d4d6
d4d8
d4f4
d4h4
EOF

piece_moves alfil-rider > "$actual_alfil"
piece_moves alfil-rider-tuple > "$actual_alfil_tuple"
piece_moves dabbaba-rider > "$actual_dabbaba"
piece_moves dabbaba-rider-tuple > "$actual_dabbaba_tuple"

cmp "$actual_alfil" "$expected_alfil"
cmp "$actual_alfil_tuple" "$expected_alfil"
cmp "$actual_dabbaba" "$expected_dabbaba"
cmp "$actual_dabbaba_tuple" "$expected_dabbaba"

echo "alfil-dabbaba-riders test OK"
