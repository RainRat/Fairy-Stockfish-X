#!/usr/bin/env bash
set -euo pipefail

ENGINE="${1:-src/stockfish}"

tmp_ini="$(mktemp)"
trap 'rm -f "$tmp_ini"' EXIT

cat >"$tmp_ini" <<'EOF'
[aries:fairy]
pieceToCharTable = -
king = -
castling = false
nMoveRule = 0
nFoldRuleImmediate = 3
nFoldValue = loss
rook = r
pushingStrength = r:8
pushFirstColor = them
pushChainEnemyOnly = true
pushCaptureAgainstFriendlyBlocker = true
pushingRemoves = shove
stepwisePushing = false
flagPiece = r
flagRegionWhite = h8
flagRegionBlack = a1
extinctionPieceTypes = r
extinctionValue = loss
startFen = 4rrrr/4rrrr/4rrrr/4rrrr/RRRR4/RRRR4/RRRR4/RRRR4 w - - 0 1
EOF

# After the shuttle a1a2, h2h1, a2a1, h1h2, a1a2, h2h1, a2a1, the side to
# move can either repeat via h1h2 or choose a non-losing move. Root search
# should avoid the repetition-losing move.
out="$("$ENGINE" <<EOF
uci
setoption name VariantPath value $tmp_ini
setoption name UCI_Variant value aries
position fen 8/8/8/8/8/8/7r/R7 w - - 0 1 moves a1a2 h2h1 a2a1 h1h2 a1a2 h2h1 a2a1
go depth 3
quit
EOF
)"
! echo "${out}" | grep -q "^bestmove h1h2$"

# Control: the move is still legal and searchable when forced.
forced="$("$ENGINE" <<EOF
uci
setoption name VariantPath value $tmp_ini
setoption name UCI_Variant value aries
position fen 8/8/8/8/8/8/7r/R7 w - - 0 1 moves a1a2 h2h1 a2a1 h1h2 a1a2 h2h1 a2a1
go depth 2 searchmoves h1h2
quit
EOF
)"
echo "${forced}" | grep -q "^bestmove h1h2$"

echo "repetition-loss search regression passed"
