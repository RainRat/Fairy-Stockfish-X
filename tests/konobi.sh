#!/usr/bin/env bash
set -euo pipefail
error() { echo "konobi regression failed on line $1"; exit 1; }
trap 'error ${LINENO}' ERR
SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ENGINE="${1:-${SCRIPT_DIR}/../src/stockfish}"
VARIANT_PATH="${2:-${SCRIPT_DIR}/../src/variants.ini}"

run_cmds() {
  cat <<EOF | "${ENGINE}" 2>/dev/null
uci
setoption name VariantPath value ${VARIANT_PATH}
setoption name UCI_Variant value konobi
$1
quit
EOF
}

out=$(run_cmds "position startpos
go perft 1")
echo "${out}" | grep -q "Nodes searched: 64"

out=$(run_cmds "position startpos moves P@b1
go perft 1")
echo "${out}" | grep -q "^P@a2: 1$"
! echo "${out}" | grep -q "^P@b1: 1$"

# Same setup as Kopano, but Konobi forbids the weak link because b3/c2/a2/b1
# still leave a strong, non-weak follow-up placement around b2.
out=$(run_cmds "position fen 8/8/8/8/3p4/8/1P6/8[Pp] w - - 0 1
go perft 1")
! echo "${out}" | grep -q "^P@c3: 1$"

# Once all strong, non-weak follow-up placements around b2 are blocked, the
# weak link becomes legal again.
out=$(run_cmds "position fen 8/8/8/8/3p4/1p6/ppp5/pp6[Pp] w - - 0 1
go perft 1")
echo "${out}" | grep -q "^P@c3: 1$"

# Existing Kopano crosscut restriction still applies.
out=$(run_cmds "position fen 8/8/8/8/2pP4/3p4/8/8[Pp] w - - 0 1
go perft 1")
! echo "${out}" | grep -q "^P@c3: 1$"

echo "konobi regression passed"
