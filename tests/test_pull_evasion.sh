#!/usr/bin/env bash
set -euo pipefail
ENGINE="${1:-./src/stockfish}"
TMP_INI=$(mktemp)
trap 'rm -f "${TMP_INI}"' EXIT
cat > "${TMP_INI}" <<'INI'
[pull-evasion:fairy]
maxFile = e
maxRank = 5
pieceToCharTable = K...A...R...k...b...r...
king = k
customPiece1 = a:mW
pullingStrength = a:3 r:1
startFen = 5/5/5/5/5 w - - 0 1
INI
run_cmds() {
  cat <<EOF | "${ENGINE}" 2>/dev/null
uci
setoption name VariantPath value ${TMP_INI}
setoption name UCI_Variant pull-evasion
isready
${1}
quit
