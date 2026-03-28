#!/usr/bin/env bash
set -euo pipefail

ENGINE="${1:-src/stockfish}"

tmp_ini="$(mktemp)"
trap 'rm -f "$tmp_ini"' EXIT

cat >"$tmp_ini" <<'EOF'
[pseudoroyal-capture-control:chess]
king = -
customPiece1 = a:W
checking = false
pseudoRoyalTypes = a
pseudoRoyalCount = 99

[pseudoroyal-capture-illegal:chess]
king = -
customPiece1 = a:W
checking = false
pseudoRoyalTypes = a
pseudoRoyalCount = 99
pseudoRoyalCaptureIllegal = true

[pseudoroyal-blast-control:chess]
king = -
customPiece1 = a:W
checking = false
pseudoRoyalTypes = a
pseudoRoyalCount = 99
blastOnCapture = true
blastCenter = true
blastDiagonals = false

[pseudoroyal-blast-illegal:chess]
king = -
customPiece1 = a:W
checking = false
pseudoRoyalTypes = a
pseudoRoyalCount = 99
pseudoRoyalCaptureIllegal = true
blastOnCapture = true
blastCenter = true
blastDiagonals = false
EOF

echo "pseudo-royal capture legality regression started"

out="$("$ENGINE" <<EOF
setoption name VariantPath value $tmp_ini
setoption name UCI_Variant value pseudoroyal-capture-control
position fen 4a3/8/8/8/8/8/4Q3/8 w - - 0 1
go perft 1
quit
EOF
)"
echo "${out}" | grep -q "^e2e8: 1$"

out="$("$ENGINE" <<EOF
setoption name VariantPath value $tmp_ini
setoption name UCI_Variant value pseudoroyal-capture-illegal
position fen 4a3/8/8/8/8/8/4Q3/8 w - - 0 1
go perft 1
quit
EOF
)"
! echo "${out}" | grep -q "^e2e8: 1$"

out="$("$ENGINE" <<EOF
setoption name VariantPath value $tmp_ini
setoption name UCI_Variant value pseudoroyal-blast-control
position fen 8/3ap3/8/8/8/8/4Q3/8 w - - 0 1
go perft 1
quit
EOF
)"
echo "${out}" | grep -q "^e2e7: 1$"

out="$("$ENGINE" <<EOF
setoption name VariantPath value $tmp_ini
setoption name UCI_Variant value pseudoroyal-blast-illegal
position fen 8/3ap3/8/8/8/8/4Q3/8 w - - 0 1
go perft 1
quit
EOF
)"
! echo "${out}" | grep -q "^e2e7: 1$"

echo "pseudo-royal capture legality regression passed"
