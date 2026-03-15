#!/bin/bash

set -euo pipefail

error() {
  echo "blast legal regression failed on line $1"
  [[ -n "${TMP1:-}" ]] && rm -f "${TMP1}"
  [[ -n "${TMP2:-}" ]] && rm -f "${TMP2}"
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-./stockfish}

run_cmds() {
  local variant_path="$1"
  local variant="$2"
  local cmds="$3"
  cat <<CMDS | "${ENGINE}"
uci
setoption name VariantPath value ${variant_path}
setoption name UCI_Variant value ${variant}
${cmds}
quit
CMDS
}

echo "blast legal regressions started"

TMP1=$(mktemp /tmp/fsx-blastblock-XXXXXX.ini)
cat >"${TMP1}" <<'INI'
[blastblock:chess]
blastOnMove = true
blastCenter = false
blastDiagonals = false
startFen = 4r1k1/8/8/8/8/8/R7/K7 w - - 0 1
INI

out=$(run_cmds "${TMP1}" "blastblock" "position startpos
go perft 1")
echo "${out}" | grep -q "^a2e2: 1$"

TMP2=$(mktemp /tmp/fsx-selfatomic-XXXXXX.ini)
cat >"${TMP2}" <<'INI'
[selfatomic:chess]
blastOnCapture = true
blastCenter = true
blastDiagonals = true
startFen = 4k3/8/8/8/8/8/4p3/4KQ2 w - - 0 1
INI

out=$(run_cmds "${TMP2}" "selfatomic" "position startpos
go perft 1")
! echo "${out}" | grep -q "^e1e2:"

rm -f "${TMP1}" "${TMP2}"
unset TMP1 TMP2

echo "blast legal regressions passed"
