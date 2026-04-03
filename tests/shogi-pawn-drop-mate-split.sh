#!/bin/bash

set -euo pipefail

error() {
  echo "shogi pawn drop mate split regression failed on line $1"
  [[ -n "${TMP_VARIANT_PATH:-}" ]] && rm -f "${TMP_VARIANT_PATH}"
  exit 1
}
trap 'error ${LINENO}' ERR

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
ENGINE="${1:-${REPO_ROOT}/src/stockfish}"

TMP_VARIANT_PATH=$(mktemp /tmp/fsx-shogi-drop-mate-split-XXXXXX.ini)
cat >"${TMP_VARIANT_PATH}" <<'INI'
[shogi-pawn-drop-split-white:minishogi]
shogiPawnDropMateIllegalWhite = true
shogiPawnDropMateIllegalBlack = false
startFen = 2k2/5/2K2/5/1R1R1[P] w - - 0 1

[shogi-pawn-drop-split-black:minishogi]
shogiPawnDropMateIllegalWhite = true
shogiPawnDropMateIllegalBlack = false
startFen = 1r1r1/5/2k2/5/2K2[p] b - - 0 1
INI

run_cmds() {
  local variant="$1"
  local cmds="$2"
  cat <<CMDS | "${ENGINE}"
uci
setoption name VariantPath value ${TMP_VARIANT_PATH}
setoption name UCI_Variant value ${variant}
${cmds}
quit
CMDS
}

echo "shogi pawn drop mate split regression tests started"

out=$(run_cmds "shogi-pawn-drop-split-white" "position startpos
go perft 1")
! echo "${out}" | grep -q "^P@c4: 1$"

out=$(run_cmds "shogi-pawn-drop-split-black" "position startpos
go perft 1")
echo "${out}" | grep -q "^P@c2: 1$"

out=$(run_cmds "shogi-pawn-drop-split-black" "setoption name Verbosity value 2
position startpos moves P@c2
go depth 1")
echo "${out}" | grep -q "info string adjudication reason checkmate result mate"
echo "${out}" | grep -q "side_to_move white"

rm -f "${TMP_VARIANT_PATH}"
unset TMP_VARIANT_PATH

echo "shogi pawn drop mate split regression tests passed"