#!/usr/bin/env bash
set -euo pipefail

ENGINE="${1:-./src/stockfish}"
VARIANTS="${2:-./src/variants.ini}"
TMP_VARIANTS="$(mktemp)"

die() {
  echo "igui regression failed on line $1" >&2
  exit 1
}
trap 'die $LINENO' ERR
trap 'rm -f "${TMP_VARIANTS}"' EXIT

cat > "${TMP_VARIANTS}" <<EOF
[igui-demo:chess]
commoner = i
customPiece1 = s:mW
iguiTypes = rs
EOF

run_cmds() {
  {
    echo "setoption name VariantPath value ${VARIANTS}"
    echo "setoption name VariantPath value ${TMP_VARIANTS}"
    echo "setoption name UCI_Variant value igui-demo"
    printf '%s\n' "$1"
    echo quit
  } | "${ENGINE}"
}

# A stationary-capture-only piece can capture an adjacent enemy without moving.
out=$(run_cmds "position fen 4k3/8/8/3p4/4S3/8/8/4K3 w - - 0 1 moves e4d5i
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/8/4S3/8/8/4K3 b - - 0 1"

# When both exist, igui notation stays distinct from an ordinary capture.
out=$(run_cmds "position fen 4k3/8/8/4p3/4R3/8/8/4K3 w - - 0 1 moves e4e5i
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/8/4R3/8/8/4K3 b - - 0 1"

out=$(run_cmds "position fen 4k3/8/8/4p3/4R3/8/8/4K3 w - - 0 1 moves e4e5
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/4R3/8/8/8/4K3 b - - 0 1"
