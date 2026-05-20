#!/bin/bash
ENGINE="./src/stockfish"
TMP_INI=$(mktemp)
cat > "${TMP_INI}" <<'INI'
[swap-pawn:fairy]
maxFile = e
maxRank = 5
king = -
adjacentSwapMoveTypes = p
startFen = 5/5/5/5/5 w - - 0 1
INI

cat <<cmds | "${ENGINE}" 2>/dev/null
uci
setoption name VariantPath value ${TMP_INI}
setoption name UCI_Variant value swap-pawn
isready
position fen 5/5/2Pb1/5/5 w - - 40 1
d
position fen 5/5/2Pb1/5/5 w - - 40 1 moves c3d3s
d
quit
cmds
rm "${TMP_INI}"
