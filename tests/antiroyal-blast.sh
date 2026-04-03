#!/bin/bash

set -euo pipefail

error() {
  echo "antiroyal blast test failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
ENGINE="${1:-${REPO_ROOT}/src/stockfish}"

tmp_ini=$(mktemp)
trap 'rm -f "${tmp_ini}"' EXIT

cat > "${tmp_ini}" <<'INI'
[antiroyal-atomic:chess]
blastOnCapture = true
castling = false
antiRoyalTypes = n
antiRoyalCount = 1
INI

perft1_nodes() {
  local fen="$1"
  cat <<CMDS | "${ENGINE}" | sed -n 's/^Nodes searched: //p' | tail -n1
uci
setoption name VariantPath value ${tmp_ini}
setoption name UCI_Variant value antiroyal-atomic
position fen ${fen}
go perft 1
quit
CMDS
}

assert_nodes() {
  local fen="$1"
  local expected="$2"
  local nodes
  nodes=$(perft1_nodes "${fen}")
  if [[ "${nodes}" != "${expected}" ]]; then
    echo "Unexpected perft(1) node count"
    echo "fen: ${fen}"
    echo "expected: ${expected}"
    echo "actual: ${nodes}"
    return 1
  fi
}

echo "antiroyal blast tests started"

# The e8 rook geometrically attacks the e2 anti-royal knight, but capturing on e2
# would explode the black king on e1. That attack must not satisfy the anti-royal
# requirement, so White should have no legal moves.
assert_nodes "4r3/8/8/8/8/8/4N3/4k1K1 w - - 0 1" "0"

# Control: once the black king is outside the blast radius, the rook attack is real
# and White regains legal king moves.
assert_nodes "4r2k/8/8/8/8/8/4N3/6K1 w - - 0 1" "5"

echo "antiroyal blast tests passed"