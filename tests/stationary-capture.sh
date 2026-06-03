#!/bin/bash

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "stationary capture regression"

load_inline_variants <<'EOF'
[stationary-capture-demo:chess]
customPiece1 = a:c^W
customPiece2 = b:mWc^K
EOF
tmp_variants="${FSX_TMP_INI}"

run_cmds() {
  run_uci "$ENGINE" "$tmp_variants" stationary-capture-demo <<EOF
$1
EOF
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
