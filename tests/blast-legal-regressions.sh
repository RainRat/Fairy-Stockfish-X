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

TMP1=$(mktemp "${TMPDIR:-/tmp}/fsx-blastblock-XXXXXX")
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

TMP2=$(mktemp "${TMPDIR:-/tmp}/fsx-selfatomic-XXXXXX")
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

TMP3=$(mktemp "${TMPDIR:-/tmp}/fsx-immobilityblast-XXXXXX")
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

TMP4=$(mktemp "${TMPDIR:-/tmp}/fsx-antimatter-XXXXXX")
cat >"${TMP4}" <<'INI'
[antimatter:chess]
blastOnSameTypeCapture = true
blastOrthogonals = false
blastDiagonals = false
INI

out=$(python3 - "${ENGINE}" "${TMP4}" <<'PY'
import subprocess
import sys

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
    "position startpos moves g2g3\n"
    "go perft 1\n"
    "quit\n"
)
proc.stdin.write(script)
proc.stdin.flush()
try:
    stdout, _ = proc.communicate(timeout=20)
except subprocess.TimeoutExpired:
    proc.kill()
    stdout, _ = proc.communicate()
    sys.stdout.write(stdout)
    sys.stderr.write("engine did not terminate within timeout\\n")
    sys.exit(1)

sys.stdout.write(stdout)
sys.exit(proc.returncode)
PY
)
grep -Fxq "Nodes searched: 20" <<<"$out"

rm -f "${TMP4}"
unset TMP4

TMP5=$(mktemp "${TMPDIR:-/tmp}/fsx-moverblast-XXXXXX")
cat >"${TMP5}" <<'INI'
[moverblast:chess]
king = -
commoner = k
blastOnCapture = true
blastOnCaptureMoverCenter = true
blastCenter = false
blastDiagonals = false
startFen = 4k3/8/8/8/8/8/3rp3/4Q2K w - - 0 1

[riflemoverblast:chess]
king = -
commoner = k
rifleCapture = true
blastOnCapture = true
blastOnCaptureMoverCenter = true
blastCenter = false
blastDiagonals = false
startFen = 4k3/8/8/8/8/8/3rp3/4Q2K w - - 0 1
INI

# A normal mover-centered capture blasts around the mover's destination.
out=$(run_cmds "${TMP5}" "moverblast" "position startpos moves e1e2
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/8/8/8/4Q3/7K b - - 0 1"

# A rifle mover-centered capture still blasts around the stationary shooter.
out=$(run_cmds "${TMP5}" "riflemoverblast" "position startpos moves e1e2
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/8/8/8/3r4/4Q2K b - - 0 1"

rm -f "${TMP5}"
unset TMP5

TMP6=$(mktemp "${TMPDIR:-/tmp}/fsx-blastcheck-XXXXXX")
cat >"${TMP6}" <<'INI'
[blastcheck:chess]
checking = false
blastOnCapture = true
blastCenter = true
blastDiagonals = false
# Black king is off the queen's attack line so the evasion only tests the blast.
startFen = 3pr3/8/8/8/8/8/3Q4/k3K3 w - - 0 1
INI

# Capturing the d8 pawn detonates the checking rook on e8, so the move must
# remain legal even though it is not a direct capture of the checker.
out=$(run_cmds "${TMP6}" "blastcheck" "position startpos
go perft 1")
echo "${out}" | grep -q "^d2d8: 1$"

rm -f "${TMP6}"
unset TMP6

echo "blast legal regressions passed"
