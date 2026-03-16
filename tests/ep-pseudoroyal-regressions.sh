#!/usr/bin/env bash
set -euo pipefail

ENGINE="${1:-src/stockfish}"

tmp_ini="$(mktemp)"
trap 'rm -f "$tmp_ini"' EXIT

cat >"$tmp_ini" <<'EOF'
[eppseudo:chess]
customPiece1 = a:W
pseudoRoyalTypes = a
pseudoRoyalCount = 99
blastOnCapture = true
blastCenter = true
blastDiagonals = false
checking = false
EOF

echo "ep pseudo-royal regression tests started"

# Pseudo-royal adjacent only to the EP landing square should not be treated
# as exploded; the en passant capture must stay legal.
out="$("$ENGINE" <<EOF
setoption name VariantPath value $tmp_ini
setoption name UCI_Variant value eppseudo
position fen 4k3/8/2A5/3pP3/8/8/8/4K3 w - d6 0 1
go perft 1
quit
EOF
)"
echo "${out}" | grep -q "^e5d6: 1$"

# Control: a pseudo-royal adjacent to the captured pawn square is still affected,
# so the EP move should remain illegal there.
out="$("$ENGINE" <<EOF
setoption name VariantPath value $tmp_ini
setoption name UCI_Variant value eppseudo
position fen 4k3/8/8/2ApP3/8/8/8/4K3 w - d6 0 1
go perft 1
quit
EOF
)"
! echo "${out}" | grep -q "^e5d6: 1$"

echo "ep pseudo-royal regression tests passed"
