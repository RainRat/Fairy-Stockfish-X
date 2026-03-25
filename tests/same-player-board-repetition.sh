#!/bin/bash

set -euo pipefail

error() {
  echo "same-player-board-repetition regression failed on line $1"
  [[ -n "${TMP_VARIANT_PATH:-}" ]] && rm -f "${TMP_VARIANT_PATH}"
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-./stockfish}

TMP_VARIANT_PATH=$(mktemp /tmp/fsx-same-player-repeat-XXXXXX.ini)
cat >"${TMP_VARIANT_PATH}" <<'INI'
[same-player-repeat-control:chess]
startFen = 4k3/8/8/8/8/8/R7/4K3 w - - 0 1

[same-player-repeat-illegal:same-player-repeat-control]
samePlayerBoardRepetitionIllegal = true
INI

run_perft() {
  local variant="$1"
  local moves="$2"
  cat <<CMDS | "${ENGINE}"
uci
setoption name VariantPath value ${TMP_VARIANT_PATH}
setoption name UCI_Variant value ${variant}
position startpos moves ${moves}
go perft 1
quit
CMDS
}

echo "same-player-board-repetition regression tests started"

moves="a2a3 e8e7 a3a2 e7e8"

out=$(run_perft "same-player-repeat-control" "${moves}")
echo "${out}" | grep -q "^a2a3: 1$"

out=$(run_perft "same-player-repeat-illegal" "${moves}")
! echo "${out}" | grep -q "^a2a3: 1$"
echo "${out}" | grep -q "^e1d1: 1$"

rm -f "${TMP_VARIANT_PATH}"
unset TMP_VARIANT_PATH

echo "same-player-board-repetition regression tests passed"
