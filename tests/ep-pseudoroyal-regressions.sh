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

[epext:chess]
customPiece1 = a:W
blastOnCapture = true
blastCenter = true
blastDiagonals = false
checking = false
extinctionValue = loss
extinctionPieceTypes = a
extinctionOpponentPieceCount = 1
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

echo "ep extinction regression tests started"

# Extinction adjudication must use the captured pawn square as the EP blast
# center. A surviving adjacent-only-to-landing-square extinction target should
# leave the resulting position playable.
out="$("$ENGINE" <<EOF
uci
setoption name VariantPath value $tmp_ini
setoption name UCI_Variant value epext
setoption name Verbosity value 2
position fen 4k1a1/8/2A5/3pP3/8/8/8/4K3 w - d6 0 1 moves e5d6
go depth 1
quit
EOF
)"
echo "${out}" | grep -q "^bestmove g8g7$"
! echo "${out}" | grep -q "adjudication reason game_end"

# Control: an extinction target adjacent to the captured pawn square must still
# be removed by the EP blast, producing the immediate extinction result.
out="$("$ENGINE" <<EOF
uci
setoption name VariantPath value $tmp_ini
setoption name UCI_Variant value epext
setoption name Verbosity value 2
position fen 4k1a1/8/8/2ApP3/8/8/8/4K3 w - d6 0 1 moves e5d6
go depth 1
quit
EOF
)"
echo "${out}" | grep -q "adjudication reason game_end"
echo "${out}" | grep -q "^bestmove (none)$"

echo "ep pseudo-royal regression tests passed"
