#!/bin/bash

set -euo pipefail

error() {
  echo "material hand cache test failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
ENGINE="${1:-${REPO_ROOT}/src/stockfish}"

echo "material hand cache test started"

same_process=$(
cat <<'EOF' | "${ENGINE}" 2>&1
uci
setoption name UCI_Variant value crazyhouse
position fen 4k3/8/8/8/8/8/8/4K3[] w - - 0 1
eval
position fen 4k3/8/8/8/8/8/8/4K3[QQQ] w - - 0 1
eval
quit
EOF
)

fresh_process=$(
cat <<'EOF' | "${ENGINE}" 2>&1
uci
setoption name UCI_Variant value crazyhouse
position fen 4k3/8/8/8/8/8/8/4K3[QQQ] w - - 0 1
eval
quit
EOF
)

same_final=$(printf '%s\n' "${same_process}" | grep "Final evaluation" | tail -n 1)
fresh_final=$(printf '%s\n' "${fresh_process}" | grep "Final evaluation" | tail -n 1)

if [ -z "${same_final}" ] || [ -z "${fresh_final}" ] || [ "${same_final}" != "${fresh_final}" ]; then
  echo "same-process:"
  echo "${same_process}"
  echo "fresh-process:"
  echo "${fresh_process}"
  exit 1
fi

echo "material hand cache test passed"