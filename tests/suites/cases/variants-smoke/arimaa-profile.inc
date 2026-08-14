#!/bin/bash

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/../../../../tests/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "Arimaa profile"

if ! probe_variant_available "$ENGINE" arimaa "$VARIANT_PATH"; then
    echo "skip: Arimaa profile requires an arimaa=yes build"
    exit 0
fi

out=$(run_uci "$ENGINE" "$VARIANT_PATH" arimaa <<'EOF'
position startpos
go perft 1
EOF
)
assert_nodes "$out" "96"
assert_contains "$out" "^R@a1: 1$"

# Once setup is supplied, the perft root is made of complete-turn candidates.
# Prefixes are retained as legal early-ending turns, so this count includes
# one-, two-, three-, and four-step turns.
out=$(run_uci "$ENGINE" "$VARIANT_PATH" arimaa <<'EOF'
position fen cdhmehdc/rrrrrrrr/8/8/8/8/RRRRRRRR/CDHMEHDC w - - 0 1
go perft 1
EOF
)
assert_nodes "$out" "20652"
assert_contains "$out" "^a2a3: 1$"
assert_not_contains "$out" "^0000:"

out=$(run_uci "$ENGINE" "$VARIANT_PATH" arimaa <<'EOF'
position fen cdhmehdc/rrrrrrrr/8/8/8/8/RRRRRRRR/CDHMEHDC w - - 0 1
go depth 1
EOF
)
assert_contains "$out" "^bestmove [a-h][1-8]"
assert_not_contains "$out" "^bestmove (none)"

# A single protocol step is an early-completed turn. Arimaa FEN remains at a
# turn boundary and never exposes the engine's internal continuation state.
out=$(run_uci "$ENGINE" "$VARIANT_PATH" arimaa <<'EOF'
position fen 8/7r/8/8/8/8/R7/8 w - - 0 1 moves a2a3
d
EOF
)
assert_contains "$out" "^Fen: 8/7r/8/8/8/R7/8/8\[\] b - - 1 1$"

# Multiple steps in one token are accepted with commas, including the
# semicolon form for clients that want an unambiguous separator.
out=$(run_uci "$ENGINE" "$VARIANT_PATH" arimaa <<'EOF'
position fen 8/7r/8/8/8/8/RR6/8 w - - 0 1 moves a2a3,b2b3
d
EOF
)
assert_contains "$out" "^Fen: 8/7r/8/8/8/RR6/8/8\[\] b - - 1 1$"

out=$(run_uci "$ENGINE" "$VARIANT_PATH" arimaa <<'EOF'
position fen 8/7r/8/8/8/8/RR6/8 w - - 0 1 moves a2a3;b2b3
d
EOF
)
assert_contains "$out" "^Fen: 8/7r/8/8/8/RR6/8/8\[\] b - - 1 1$"

# A stronger adjacent enemy freezes an unsupported rabbit.
out=$(run_uci "$ENGINE" "$VARIANT_PATH" arimaa <<'EOF'
position fen 8/8/8/8/8/3Re4/8/8 w - - 0 1
go perft 1
EOF
)
assert_nodes "$out" "0"

# Friendly orthogonal support prevents freezing.
out=$(run_uci "$ENGINE" "$VARIANT_PATH" arimaa <<'EOF'
position fen 8/8/8/8/8/2Re4/2R5/8 w - - 0 1
go perft 1
EOF
)
assert_contains "$out" "^c3b3: 1$"

# Equal-strength pieces do not freeze each other.
out=$(run_uci "$ENGINE" "$VARIANT_PATH" arimaa <<'EOF'
position fen 8/8/8/8/8/3Ee3/8/8 w - - 0 1
go perft 1
EOF
)
assert_contains "$out" "^d3d2: 1$"

# The generic push encoding exposes an elephant push of a rabbit.
out=$(run_uci "$ENGINE" "$VARIANT_PATH" arimaa <<'EOF'
position fen 8/8/8/8/8/3Er4/8/8 w - - 0 1
go perft 1
EOF
)
assert_contains "$out" "^d3d2,e3: 1$"

# A push's internal comma is not confused with the comma starting the next
# Arimaa step.
out=$(run_uci "$ENGINE" "$VARIANT_PATH" arimaa <<'EOF'
position fen 8/8/8/8/8/3Er3/2R5/8 w - - 0 1 moves d3d2,e3,c2b2
d
EOF
)
assert_contains "$out" "^Fen: 8/8/8/8/8/3r4/1R1E4/8\[\] b - - 1 1$"

# Unprotected trap squares remove a piece after the step.
out=$(run_uci "$ENGINE" "$VARIANT_PATH" arimaa <<'EOF'
position fen 8/8/8/8/8/8/2R5/8 w - - 0 1 moves c2c3
d
EOF
)
assert_contains "$out" "Fen: 8/8/8/8/8/8/8/8\[\] b - - 0 1"
