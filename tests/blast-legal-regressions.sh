#!/bin/bash

set -euo pipefail

error() {
  echo "blast legal regression failed on line $1"
  [[ -n "${TMP1:-}" ]] && rm -f "${TMP1}"
  [[ -n "${TMP2:-}" ]] && rm -f "${TMP2}"
  [[ -n "${TMP3:-}" ]] && rm -f "${TMP3}"
  [[ -n "${TMP4:-}" ]] && rm -f "${TMP4}"
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

TMP3=$(mktemp /tmp/fsx-immobilityblast-XXXXXX.ini)
cat >"${TMP3}" <<'INI'
[immobilityblast:chess]
king = -
commoner = k
immobilityIllegal = true
blastOnSameTypeCapture = true
selfCapture = true
mandatoryPawnPromotion = false
startFen = 1P6/P7/8/8/8/8/8/K7 w - - 0 1
INI

out=$(run_cmds "${TMP3}" "immobilityblast" "position startpos
go perft 1")
echo "${out}" | grep -q "^a7b8: 1$"

rm -f "${TMP3}"
unset TMP3

TMP4=$(mktemp /tmp/fsx-antimatter-XXXXXX.ini)
cat >"${TMP4}" <<'INI'
[antimatter:chess]
blastOnSameTypeCapture = true
blastOrthogonals = false
blastDiagonals = false
INI

out=$(python3 - "${ENGINE}" "${TMP4}" <<'PY'
import subprocess
import sys
import time

engine = sys.argv[1]
variant_path = sys.argv[2]

proc = subprocess.Popen(
    [engine],
    stdin=subprocess.PIPE,
    stdout=subprocess.PIPE,
    stderr=subprocess.STDOUT,
    text=True,
)

script = (
    "uci\n"
    f"setoption name VariantPath value {variant_path}\n"
    "setoption name UCI_Variant value antimatter\n"
    "setoption name UCI_AnalyseMode value true\n"
    "position startpos moves g2g3\n"
    "go infinite\n"
)
proc.stdin.write(script)
proc.stdin.flush()
time.sleep(1.0)
proc.stdin.write("stop\nquit\n")
proc.stdin.flush()
stdout, _ = proc.communicate(timeout=10)
sys.stdout.write(stdout)
sys.exit(proc.returncode)
PY
)
echo "${out}" | grep -q "^bestmove "

rm -f "${TMP4}"
unset TMP4

echo "blast legal regressions passed"
