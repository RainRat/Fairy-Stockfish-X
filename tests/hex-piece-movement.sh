#!/usr/bin/env bash
set -euo pipefail

error() {
  echo "hex piece movement regression failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE="${1:-./src/stockfish}"

TMP_VARIANT_PATH=$(mktemp /tmp/fsx-hex-pieces-XXXXXX.ini)
trap 'rm -f "${TMP_VARIANT_PATH}"' EXIT

cat >"${TMP_VARIANT_PATH}" <<'INI'
[hex-rook-test:fairy]
maxRank = 5
maxFile = e
hexBoard = true
pieceToCharTable = RKBQ.rkbq
king = -
customPiece1 = r:RrfBlbB
customPiece2 = k:WrfFlbF
customPiece3 = b:flBrbBrf(2,1)lb(2,1)fr(2,1)bl(2,1)
customPiece4 = q:RrfBlbBflBrbBrf(2,1)lb(2,1)fr(2,1)bl(2,1)
startFen = 5/5/2R2/5/5 w - - 0 1

[hex-king-test:hex-rook-test]
startFen = 5/5/2K2/5/5 w - - 0 1

[hex-bishop-test:hex-rook-test]
startFen = 5/5/2B2/5/5 w - - 0 1

[hex-queen-test:hex-rook-test]
startFen = 5/5/2Q2/5/5 w - - 0 1
INI

run_cmds() {
  cat <<EOF | "${ENGINE}" 2>/dev/null
uci
setoption name VariantPath value ${TMP_VARIANT_PATH}
setoption name UCI_Variant value $1
$2
quit
EOF
}

out=$(run_cmds "hex-rook-test" "position startpos
go perft 1")
echo "${out}" | grep -q "Nodes searched: 12"
echo "${out}" | grep -q "^c3a1: 1$"
echo "${out}" | grep -q "^c3c1: 1$"
echo "${out}" | grep -q "^c3b2: 1$"
echo "${out}" | grep -q "^c3c2: 1$"
echo "${out}" | grep -q "^c3a3: 1$"
echo "${out}" | grep -q "^c3b3: 1$"
echo "${out}" | grep -q "^c3d3: 1$"
echo "${out}" | grep -q "^c3e3: 1$"
echo "${out}" | grep -q "^c3c4: 1$"
echo "${out}" | grep -q "^c3d4: 1$"
echo "${out}" | grep -q "^c3c5: 1$"
echo "${out}" | grep -q "^c3e5: 1$"

out=$(run_cmds "hex-king-test" "position startpos
go perft 1")
echo "${out}" | grep -q "Nodes searched: 6"
echo "${out}" | grep -q "^c3b2: 1$"
echo "${out}" | grep -q "^c3c2: 1$"
echo "${out}" | grep -q "^c3b3: 1$"
echo "${out}" | grep -q "^c3d3: 1$"
echo "${out}" | grep -q "^c3c4: 1$"
echo "${out}" | grep -q "^c3d4: 1$"

out=$(run_cmds "hex-bishop-test" "position startpos
go perft 1")
echo "${out}" | grep -q "Nodes searched: 8"
echo "${out}" | grep -q "^c3b1: 1$"
echo "${out}" | grep -q "^c3e1: 1$"
echo "${out}" | grep -q "^c3a2: 1$"
echo "${out}" | grep -q "^c3d2: 1$"
echo "${out}" | grep -q "^c3b4: 1$"
echo "${out}" | grep -q "^c3e4: 1$"
echo "${out}" | grep -q "^c3a5: 1$"
echo "${out}" | grep -q "^c3d5: 1$"

out=$(run_cmds "hex-queen-test" "position startpos
go perft 1")
echo "${out}" | grep -q "Nodes searched: 20"
echo "${out}" | grep -q "^c3a1: 1$"
echo "${out}" | grep -q "^c3e5: 1$"
echo "${out}" | grep -q "^c3b1: 1$"
echo "${out}" | grep -q "^c3e1: 1$"
echo "${out}" | grep -q "^c3a2: 1$"
echo "${out}" | grep -q "^c3d5: 1$"

echo "hex piece movement regression passed"
