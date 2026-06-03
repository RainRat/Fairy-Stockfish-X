#!/bin/bash

set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "move-morph regression"
VARIANTS=${2:-src/variants.ini}

out=$(run_uci "$ENGINE" "$VARIANTS" bishop-knight-morph-factor <<'EOF'
position startpos moves g1f3
d
EOF
)
echo "${out}" | grep -q "Fen: rnbqkbnr/pppppppp/8/8/8/5B2/PPPPPPPP/RNBQKB1R b KQkq - 1 1"

out=$(run_uci "$ENGINE" "$VARIANTS" bishop-knight-morph-factor <<'EOF'
position fen 4k3/8/8/8/8/8/8/2B1K3 w - - 0 1 moves c1g5
d
EOF
)
echo "${out}" | grep -q "Fen: 4k3/8/8/6N1/8/8/8/4K3 b - - 1 1"
