#!/bin/bash

set -euo pipefail

error() {
  echo "drop legality split regression failed on line $1"
  [[ -n "${TMP_VARIANT_PATH:-}" ]] && rm -f "${TMP_VARIANT_PATH}"
  exit 1
}
trap 'error ${LINENO}' ERR

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
ENGINE="${1:-${REPO_ROOT}/src/stockfish}"

TMP_VARIANT_PATH=$(mktemp /tmp/fsx-drop-split-XXXXXX.ini)
cat >"${TMP_VARIANT_PATH}" <<'INI'
[dropcheck-split-white:chess]
pieceDrops = true
dropChecksWhite = false
dropChecksBlack = true
startFen = 4k3/8/8/8/8/8/8/4K3[R] w - - 0 1

[dropcheck-split-black:chess]
pieceDrops = true
dropChecksWhite = false
dropChecksBlack = true
startFen = 4k3/8/8/8/8/8/8/4K3[r] b - - 0 1

[dropmate-split-white:chess]
pieceDrops = true
dropChecks = true
dropMatesWhite = false
dropMatesBlack = true
startFen = 4k3/8/4K3/8/8/8/8/8[Q] w - - 0 1

[dropmate-split-black:chess]
pieceDrops = true
dropChecks = true
dropMatesWhite = false
dropMatesBlack = true
startFen = 8/8/8/8/8/4k3/8/4K3[q] b - - 0 1

[dropnodoubled-split-white:chess]
pieceDrops = true
dropNoDoubledWhite = p
startFen = 4k3/8/8/8/8/8/4P3/4K3[P] w - - 0 1

[dropnodoubled-split-black:chess]
pieceDrops = true
dropNoDoubledWhite = p
startFen = 4k3/4p3/8/8/8/8/8/4K3[p] b - - 0 1

[dropnodoubledcount-split-white:chess]
pieceDrops = true
dropNoDoubled = p
dropNoDoubledCountWhite = 2
startFen = 4k3/8/8/8/8/8/4P3/4K3[P] w - - 0 1

[dropnodoubledcount-split-black:chess]
pieceDrops = true
dropNoDoubled = p
dropNoDoubledCountWhite = 2
startFen = 4k3/4p3/8/8/8/8/8/4K3[p] b - - 0 1

[pathway-drop-rule]
maxRank = 6
maxFile = 6
immobile = p
pieceDrops = true
mustDrop = true
checking = false
doubleStep = false
castling = false
nMoveRule = 0
stalemateValue = win
pathwayDropRule = true
startFen = 6/6/6/6/6/6[Pp] w - - 0 1
INI

run_perft() {
  local variant="$1"
  cat <<CMDS | "${ENGINE}"
uci
setoption name VariantPath value ${TMP_VARIANT_PATH}
setoption name UCI_Variant value ${variant}
position startpos
go perft 1
quit
CMDS
}

echo "drop legality split regression tests started"

out=$(run_perft "dropcheck-split-white")
! echo "${out}" | grep -q "^R@e7: 1$"

out=$(run_perft "dropcheck-split-black")
echo "${out}" | grep -q "^R@e2: 1$"

out=$(run_perft "dropmate-split-white")
! echo "${out}" | grep -q "^Q@e7: 1$"

out=$(run_perft "dropmate-split-black")
echo "${out}" | grep -q "^Q@e2: 1$"

out=$(run_perft "dropnodoubled-split-white")
! echo "${out}" | grep -q "^P@e4: 1$"

out=$(run_perft "dropnodoubled-split-black")
echo "${out}" | grep -q "^P@e5: 1$"

out=$(run_perft "dropnodoubledcount-split-white")
echo "${out}" | grep -q "^P@e4: 1$"

out=$(run_perft "dropnodoubledcount-split-black")
! echo "${out}" | grep -q "^P@e5: 1$"

out=$(cat <<CMDS | "${ENGINE}"
uci
setoption name VariantPath value ${TMP_VARIANT_PATH}
setoption name UCI_Variant value pathway-drop-rule
position fen 6/6/6/6/3p2/6[Pp] w - - 0 1
go perft 1
quit
CMDS
)
! echo "${out}" | grep -q "^P@c2: 1$"

out=$(cat <<CMDS | "${ENGINE}"
uci
setoption name VariantPath value ${TMP_VARIANT_PATH}
setoption name UCI_Variant value pathway-drop-rule
position fen 6/6/6/2P3/3p2/6[Pp] w - - 0 1
go perft 1
quit
CMDS
)
echo "${out}" | grep -q "^P@c2: 1$"

rm -f "${TMP_VARIANT_PATH}"
unset TMP_VARIANT_PATH

echo "drop legality split regression tests passed"