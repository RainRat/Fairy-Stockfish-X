#!/bin/bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "seega regression"

echo "seega regression started"

out=$(run_uci "$ENGINE" "$VARIANTS" seega <<'EOF'
position startpos moves D@a1
go perft 1
EOF
)
echo "${out}" | grep -q "^0000: 1$"
! echo "${out}" | grep -q "^D@"

out=$(run_uci "$ENGINE" "$VARIANTS" seega <<'EOF'
position startpos moves D@a1 0000
go perft 1
EOF
)
echo "${out}" | grep -q "^D@"
! echo "${out}" | grep -q "^0000: 1$"

out=$(run_uci "$ENGINE" "$VARIANTS" seega <<'EOF'
position startpos moves D@a1 0000 D@b1
d
EOF
)
echo "${out}" | grep -Eq "^Fen: .* b "

out=$(run_uci "$ENGINE" "$VARIANTS" seega <<'EOF'
position fen d4/5/1D1dD/5/d4 w - - 0 1 moves b3c3
d
EOF
)
echo "${out}" | grep -Eq "Fen: d4/5/2D1D/5/d4(\\[\\])? b - - 1 1"

out=$(run_uci "$ENGINE" "$VARIANTS" seega <<'EOF'
position fen 5/2D2/1DdD1/D1D2/dD3 b - - 0 1
go perft 1
EOF
)
echo "${out}" | grep -q "^0000: 1$"

out=$(run_uci "$ENGINE" "$VARIANTS" seega <<'EOF'
position fen 5/5/5/5/1D3[] b - - 0 1
go movetime 20
EOF
)
echo "${out}" | grep -q "^info depth 0 score mate 0$"
echo "${out}" | grep -q "^bestmove (none)$"

echo "seega regression passed"
