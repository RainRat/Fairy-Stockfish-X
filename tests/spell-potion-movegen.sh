#!/bin/bash

set -euo pipefail

error() {
  echo "spell potion movegen test failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
ENGINE="${1:-${REPO_ROOT}/src/stockfish}"

echo "spell potion movegen test started"

out=$(cat <<'EOF' | "${ENGINE}" 2>&1
uci
setoption name UCI_Variant value spell-chess
position fen 7k/P7/8/8/8/8/8/4K3[JFj] w - - 0 1
go perft 1
quit
EOF
)

for move in a7a8q a7a8r a7a8b a7a8n e1d1 e1f1 e1d2 e1e2 e1f2; do
  if [ "$(printf '%s\n' "${out}" | grep -c "^${move}: 1$")" -ne 1 ]; then
    echo "${out}"
    exit 1
  fi
done

for gated in "f@g7,e1d1" "f@h8,e1f2"; do
  if ! printf '%s\n' "${out}" | grep -q "^${gated}: 1$"; then
    echo "${out}"
    exit 1
  fi
done

echo "spell potion movegen test passed"