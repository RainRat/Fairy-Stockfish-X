#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ENGINE="${1:-${SCRIPT_DIR}/../src/stockfish}"
VARIANTS="${2:-${SCRIPT_DIR}/../src/variants.ini}"
TMP_VARIANTS="$(mktemp)"

die() {
  echo "igui regression failed on line $1" >&2
  exit 1
}
trap 'die $LINENO' ERR
trap 'rm -f "${TMP_VARIANTS}"' EXIT

cat > "${TMP_VARIANTS}" <<EOF
[stationary-capture-demo:chess]
customPiece1 = a:c^W
customPiece2 = b:mWc^K
EOF

run_cmds() {
  {
    echo "setoption name VariantPath value ${VARIANTS}"
    echo "setoption name VariantPath value ${TMP_VARIANTS}"
    echo "setoption name UCI_Variant value stationary-capture-demo"
    printf '%s\n' "$1"
    echo quit
  } | "${ENGINE}"
}

# A stationary-capture-only piece can capture an adjacent enemy without moving.
out=$(run_cmds "position fen 4k3/8/8/4p3/4A3/8/8/4K3 w - - 0 1 moves e4e5
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/8/4A3/8/8/4K3 b - - 0 1"

# A mixed piece can move by W but capture adjacent squares without moving.
out=$(run_cmds "position fen 4k3/8/8/4p3/4B3/8/8/4K3 w - - 0 1 moves e4e5
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/8/4B3/8/8/4K3 b - - 0 1"

out=$(run_cmds "position fen 4k3/8/8/8/4B3/8/8/4K3 w - - 0 1 moves e4e5
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/4B3/8/8/8/4K3 b - - 1 1"
