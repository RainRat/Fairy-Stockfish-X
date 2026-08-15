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

# Standard setup keeps the placer on move until that side's pocket is empty.
out=$(run_uci "$ENGINE" "$VARIANT_PATH" arimaa <<'EOF'
position startpos moves R@a1
d
EOF
)
assert_contains "$out" "^Fen: 8/8/8/8/8/8/8/R7\\[.*\\] w - - 0 1$"

out=$(run_uci "$ENGINE" "$VARIANT_PATH" arimaa <<'EOF'
position startpos
go depth 8
EOF
)
assert_contains "$out" "^bestmove [A-Z]@[a-h][1-8]$"

# Completing both setup pockets changes the side to move and enters the first
# ordinary complete turn. This exercises the final placement on each side.
out=$(run_uci "$ENGINE" "$VARIANT_PATH" arimaa <<'EOF'
position startpos moves R@a1 R@b1 R@c1 R@d1 R@e1 R@f1 R@g1 R@h1 C@a2 C@b2 D@c2 D@d2 H@e2 H@f2 M@g2 E@h2 R@a8 R@b8 R@c8 R@d8 R@e8 R@f8 R@g8 R@h8 C@a7 C@b7 D@c7 D@d7 H@e7 H@f7 M@g7 E@h7
d
EOF
)
assert_contains "$out" "^Fen: rrrrrrrr/ccddhhme/8/8/8/8/CCDDHHME/RRRRRRRR\\[\\] w - - 0 1$"

out=$(run_uci "$ENGINE" "$VARIANT_PATH" arimaa <<'EOF'
position startpos moves R@a1 R@b1 R@c1 R@d1 R@e1 R@f1 R@g1 R@h1 C@a2 C@b2 D@c2 D@d2 H@e2 H@f2 M@g2 E@h2 R@a8 R@b8 R@c8 R@d8 R@e8 R@f8 R@g8 R@h8 C@a7 C@b7 D@c7 D@d7 H@e7 H@f7 M@g7 E@h7
go depth 1
EOF
)
assert_contains "$out" "^bestmove [a-h][1-8]"

# Native state coverage verifies setup does not advance gamePly or rule50.
"${ROOT_DIR}/tests/native/engine-rules.sh" "$ENGINE" "$VARIANT_PATH" arimaa-setup

# Whole-turn root restrictions are honored by the Arimaa search path.
out=$(run_uci "$ENGINE" "$VARIANT_PATH" arimaa <<'EOF'
position fen cdhmehdc/rrrrrrrr/8/8/8/8/RRRRRRRR/CDHMEHDC w - - 0 1
go depth 1 searchmoves a2a3
EOF
)
assert_contains "$out" "^bestmove a2a3$"

# Active node and time limits must stop ordinary turn search, not just setup
# parsing or invalid root restrictions.
out=$(run_uci "$ENGINE" "$VARIANT_PATH" arimaa <<'EOF'
position fen cdhmehdc/rrrrrrrr/8/8/8/8/RRRRRRRR/CDHMEHDC w - - 0 1
go nodes 1
EOF
)
assert_contains "$out" "^bestmove [a-h][1-8]"

out=$(run_uci "$ENGINE" "$VARIANT_PATH" arimaa <<'EOF'
position fen cdhmehdc/rrrrrrrr/8/8/8/8/RRRRRRRR/CDHMEHDC w - - 0 1
go movetime 1
EOF
)
assert_contains "$out" "^bestmove [a-h][1-8]"

out=$(run_uci "$ENGINE" "$VARIANT_PATH" arimaa <<'EOF'
position fen cdhmehdc/rrrrrrrr/8/8/8/8/RRRRRRRR/CDHMEHDC w - - 0 1
go depth 1 searchmoves invalid
EOF
)
assert_contains "$out" "^bestmove \\(none\\)$"

out=$(run_uci "$ENGINE" "$VARIANT_PATH" arimaa <<'EOF'
position startpos
go movetime 1 searchmoves invalid
EOF
)
assert_contains "$out" "^bestmove \\(none\\)$"

# A completed-turn boundary with a rabbit already extinct is terminal even
# when the side to move still has an ordinary physical move available.
out=$(run_uci "$ENGINE" "$VARIANT_PATH" arimaa <<'EOF'
setoption name Verbosity value 1
position fen 8/8/8/3Dr3/8/8/R7/8 w - - 0 1
go depth 1 searchmoves a2a3
EOF
)
assert_contains "$out" "^bestmove \\(none\\)$"

out=$(run_uci "$ENGINE" "$VARIANT_PATH" arimaa <<'EOF'
position startpos
go depth 1 searchmoves invalid
EOF
)
assert_contains "$out" "^bestmove \\(none\\)$"

out=$(run_uci "$ENGINE" "$VARIANT_PATH" arimaa <<'EOF'
position fen cdhmehdc/rrrrrrrr/8/8/8/8/RRRRRRRR/CDHMEHDC w - - 0 1
banmoves a2a3
go depth 1
EOF
)
assert_not_contains "$out" "^bestmove a2a3$"

# Incremental setup keys agree with a freshly parsed position whose pocket has
# already lost the placed rabbit, even though the side to move is unchanged.
key_out=$(run_uci "$ENGINE" "$VARIANT_PATH" arimaa <<'EOF'
position startpos moves R@a1
d
EOF
)
setup_key=$(sed -n 's/^Key: //p' <<<"$key_out")
fresh_out=$(run_uci "$ENGINE" "$VARIANT_PATH" arimaa <<'EOF'
position fen 8/8/8/8/8/8/8/R7[EMHHDDCCRRRRRRRemhhddccrrrrrrrr] w - - 0 1
d
EOF
)
assert_contains "$fresh_out" "^Key: $setup_key$"

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
assert_not_contains "$out" "^bestmove \\(none\\)"

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
position fen 8/7r/8/8/8/3Re4/8/8 w - - 0 1
go perft 1
EOF
)
assert_nodes "$out" "0"

# Friendly orthogonal support prevents freezing.
out=$(run_uci "$ENGINE" "$VARIANT_PATH" arimaa <<'EOF'
position fen 8/7r/8/8/8/2Re4/2R5/8 w - - 0 1
go perft 1
EOF
)
assert_contains "$out" "^c3b3: 1$"

# Equal-strength pieces do not freeze each other.
out=$(run_uci "$ENGINE" "$VARIANT_PATH" arimaa <<'EOF'
position fen 8/7r/8/8/8/3Ee3/R7/8 w - - 0 1
go perft 1
EOF
)
assert_contains "$out" "^d3d2: 1$"

# The compound encoding exposes an elephant pull of a rabbit.
out=$(run_uci "$ENGINE" "$VARIANT_PATH" arimaa <<'EOF'
position fen 8/7r/8/8/8/3Er4/R7/8 w - - 0 1
go perft 1
EOF
)
assert_contains "$out" "^d3d2,e3: 1$"

# A temporary return to the starting layout may be followed by a real step,
# but a completed turn may not be a whole-turn pass.
out=$(run_uci "$ENGINE" "$VARIANT_PATH" arimaa <<'EOF'
position fen 8/7r/8/8/8/8/R1R5/8 w - - 0 1 moves a2b2,b2a2,a2a3
d
EOF
)
assert_contains "$out" "^Fen: 8/7r/8/8/8/R7/2R5/8\[\] b - - 1 1$"

out=$(run_uci "$ENGINE" "$VARIANT_PATH" arimaa <<'EOF'
position fen 8/7r/8/8/8/8/R1R5/8 w - - 0 1
go perft 1
EOF
)
assert_not_contains "$out" "^a2b2,b2a2: 1$"

# The same whole-turn no-op is rejected by protocol replay without crashing.
out=$(run_uci "$ENGINE" "$VARIANT_PATH" arimaa <<'EOF'
position fen 8/7r/8/8/8/8/R1R5/8 w - - 0 1 moves a2b2,b2a2
d
EOF
)
assert_contains "$out" "^Fen: 8/7r/8/8/8/8/R1R5/8\\[\\] w - - 0 1$"

# Goal positions are terminal at the next turn boundary.
out=$(run_uci "$ENGINE" "$VARIANT_PATH" arimaa <<'EOF'
position fen R7/8/8/8/8/8/8/8 b - - 0 1
go perft 1
EOF
)
assert_nodes "$out" "0"

# A rabbit pushed onto the opponent's goal row wins for that opponent at the
# completed boundary, even when the rabbit remains mobile after the push.
out=$(run_uci "$ENGINE" "$VARIANT_PATH" arimaa <<'EOF'
position fen C7/eR6/7r/8/8/8/8/8 b - - 0 1 moves a7b7,b8
go perft 1
EOF
)
assert_nodes "$out" "0"

# A rabbit goal is adjudicated only at the completed-turn boundary.  The
# white rabbit reaches rank 1 in a push prefix, then another black step can
# still complete the turn.
out=$(run_uci "$ENGINE" "$VARIANT_PATH" arimaa <<'EOF'
position fen 8/7r/8/8/8/8/eR6/8 b - - 0 1
go perft 1
EOF
)
assert_contains "$out" "^a2b2,b1,h7h6: 1$"

# Rabbit elimination is also a completed-turn terminal condition.
out=$(run_uci "$ENGINE" "$VARIANT_PATH" arimaa <<'EOF'
position fen 7r/8/8/8/8/8/8/E7 w - - 0 1
go perft 1
EOF
)
assert_nodes "$out" "0"

# Simultaneous rabbit elimination is won by the player who completed the turn.
out=$(run_uci "$ENGINE" "$VARIANT_PATH" arimaa <<'EOF'
position fen 8/8/8/8/8/2REr3/8/8 w - - 0 1 moves d3e3,f3
go perft 1
EOF
)
assert_nodes "$out" "0"

# The third occurrence of a boundary board for the same player is illegal as
# an early-ending turn. Continuing the same turn remains available.
out=$(run_uci "$ENGINE" "$VARIANT_PATH" arimaa <<'EOF'
position fen 8/6er/8/8/8/8/E7/R7 w - - 0 1 moves a2b2 g7f7 b2a2 f7g7 a2b2 g7f7 b2a2
go perft 1
EOF
)
assert_not_contains "$out" "^f7g7: 1$"

# A one-step protocol turn uses the same repetition validation as a compound
# token, so an illegal third occurrence is rejected during position replay.
out=$(run_uci "$ENGINE" "$VARIANT_PATH" arimaa <<'EOF'
position fen 8/6er/8/8/8/8/E7/R7 w - - 0 1 moves a2b2 g7f7 b2a2 f7g7 a2b2 g7f7 b2a2 f7g7
d
EOF
)
assert_contains "$out" "^Fen: 8/5e1r/8/8/8/8/E7/R7\\[\\] b - - 7 4$"

# Arimaa push adjacency is orthogonal; diagonal king-neighborhood squares are
# not valid pusher or pushed-piece destinations.
out=$(run_uci "$ENGINE" "$VARIANT_PATH" arimaa <<'EOF'
position fen 8/7r/8/4r3/3E4/8/R7/8 w - - 0 1
go perft 1
EOF
)
assert_not_contains "$out" "^d4e5,f5: 1$"

# A push's internal comma is not confused with the comma starting the next
# Arimaa step.
out=$(run_uci "$ENGINE" "$VARIANT_PATH" arimaa <<'EOF'
position fen 8/7r/8/8/8/3Er3/2R5/8 w - - 0 1 moves d3d2,e3,c2b2
d
EOF
)
assert_contains "$out" "^Fen: 8/7r/8/8/8/3r4/1R1E4/8\[\] b - - 1 1$"

# Unprotected trap squares remove a piece after the step.
out=$(run_uci "$ENGINE" "$VARIANT_PATH" arimaa <<'EOF'
position fen 8/7r/8/8/8/8/2R5/8 w - - 0 1 moves c2c3
d
EOF
)
assert_contains "$out" "Fen: 8/7r/8/8/8/8/8/8\[\] b - - 0 1"
