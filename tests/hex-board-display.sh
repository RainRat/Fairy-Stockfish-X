#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ENGINE="${1:-${SCRIPT_DIR}/../src/stockfish}"

error() {
  echo "hex-board display regression failed on line $1" >&2
  exit 1
}
trap 'error ${LINENO}' ERR

hex_ini=$(mktemp)
cat > "${hex_ini}" <<'INI'
[hex-display:fairy]
maxRank = 5
maxFile = e
hexBoard = true
checking = false
king = -
pieceToCharTable = -
startFen = 5/5/5/5/5 w - - 0 1
INI

out=$(cat <<EOF | "${ENGINE}"
uci
setoption name VariantPath value ${hex_ini}
setoption name UCI_Variant value hex-display
position startpos
d
quit
EOF
)

echo "${out}" | grep -Fq "   a    b    c    d    e"
echo "${out}" | grep -Fq " [  ] [  ] [  ] [  ] [  ] 5"
echo "${out}" | grep -Fq "         [  ] [  ] [  ] [  ] [  ] 1 *"
echo "${out}" | grep -Fq "           a    b    c    d    e"
echo "${out}" | grep -Fq "Fen: 5/5/5/5/5 w - - 0 1"

bad_ini=$(mktemp)
cat > "${bad_ini}" <<'INI'
[bad-hex:fairy]
hexBoard = true
cylindrical = true
startFen = 8/8/8/8/8/8/8/8 w - - 0 1
INI

bad_out=$("${ENGINE}" check "${bad_ini}" 2>&1 || true)
echo "${bad_out}" | grep -Fq "hexBoard is not supported together with cylindrical or toroidal topology."
echo "${bad_out}" | grep -Fq "Variant 'bad-hex' has invalid configuration. Skipping."

rm -f "${hex_ini}" "${bad_ini}"

echo "hex-board display regression passed"
