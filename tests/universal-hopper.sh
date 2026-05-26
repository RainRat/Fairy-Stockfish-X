#!/usr/bin/env bash

set -euo pipefail

# Test Universal Hopper features

# 1. Setup temporary variant file
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
ROOT_DIR=$(cd "${SCRIPT_DIR}/.." && pwd)
ENGINE=${1:-"${ROOT_DIR}/src/stockfish"}
if [[ ! -x "${ENGINE}" ]]; then
    if [[ -x "${ROOT_DIR}/stockfish" ]]; then
        ENGINE="${ROOT_DIR}/stockfish"
    fi
fi
if [[ ! -x "${ENGINE}" ]]; then
    echo "engine executable not found: pass path as first argument" >&2
    exit 2
fi

source "${SCRIPT_DIR}/lib/uci.sh"

INI_FILE=$(mktemp -t universal_hopper_test.XXXXXX.ini)
trap 'rm -f "${INI_FILE}"' EXIT
cat << 'EOF' > "$INI_FILE"
[hopper-common:chess]
pieceToCharTable = PNBRQKDFGHS

[hopper-base:hopper-common]
customPiece1 = d:{hurdles: 1,1; pre: 1,*; post: 1,1}Q
# d is a Grasshopper

[directional-hopper:hopper-common]
customPiece1 = d:f{hurdles: 1,1; pre: 1,*; post: 1,1}R
# d can only jump forward orthogonally

[locust-first:hopper-common]
customPiece1 = d:{hurdles: 1,1; pre: 1,*; post: 1,1; capture: locust_first}R

[locust-king:hopper-common]
customPiece1 = d:{hurdles: 1,1; pre: 1,*; post: 1,1; capture: locust_first}R

[piece-type-hurdles:hopper-common]
customPiece1 = d:{hurdles: 1,1; pre: 2,2; post: 1,1; capture: locust_first; hurdle_piece_types: n; transparent_piece_types: p}R

[locust-all:hopper-common]
customPiece1 = d:{hurdles: 2,2; pre: 1,*; post: 1,1; capture: locust_all}R

[locust-all-friendly-mix:hopper-common]
customPiece1 = d:c{hurdles: 2,2; pre: 1,*; post: 1,1; capture: locust_all; hurdle_types: enemy,friendly}R

[locust-last:hopper-common]
customPiece1 = d:c{hurdles: 2,2; pre: 1,1; post: 1,1; capture: locust_last; hurdle_types: enemy}R

[locust-friendly-hurdle:hopper-common]
customPiece1 = d:c{hurdles: 1,1; pre: 1,1; post: 1,1; capture: locust_first; hurdle_types: friendly}R

[locust-friendly-selfcapture:hopper-common]
customPiece1 = d:c{hurdles: 1,1; pre: 1,1; post: 1,1; capture: locust_first; hurdle_types: friendly}R
selfCapture = true

[dest-capture-hopper:hopper-common]
customPiece1 = d:c{hurdles: 1,1; pre: 1,1; post: 1,1; capture: dest; hurdle_types: enemy}R

[dest-capture-no-hurdle:hopper-common]
customPiece1 = d:c{hurdles: 1,1; pre: 1,1; post: 1,1; capture: dest; hurdle_types: enemy}R

[equi-hopper:hopper-common]
customPiece1 = d:{hurdles: 1,1; equi: hopper}Q

[equi-stopper:hopper-common]
customPiece1 = d:{hurdles: 1,1; equi: stopper}Q

[equi-stopper-multi:hopper-common]
customPiece1 = d:{hurdles: 2,2; equi: stopper}Q

[equi-stopper-pre3:hopper-common]
customPiece1 = d:{hurdles: 1,1; pre: 3,3; post: 1,1; equi: stopper}Q

[wrapped-hopper:hopper-common]
topology = cylinder
customPiece1 = d:{hurdles: 1,1; pre: 1,*; post: 1,1}R

[wrapped-initial-locust:hopper-common]
topology = cylinder
pieceToCharTable = PNBRQKDFGHSA
customPiece1 = a:i{hurdles: 1,1; pre: 1,1; post: 1,1; capture: locust_first; hurdle_types: enemy}R
doubleStepRegionWhite = A(e2)

[wrapped-locust-all:hopper-common]
topology = cylinder
customPiece1 = d:c{hurdles: 2,2; pre: 1,1; post: 1,1; capture: locust_all; hurdle_types: enemy}R

[locust-all-hand:crazyhouse]
pieceToCharTable = PNBRQK....D.....pnbrqk....d.....
customPiece1 = d:c{hurdles: 2,2; pre: 1,*; post: 1,1; capture: locust_all; hurdle_types: enemy}R
startFen = 7k/8/8/3p4/3p4/3D4/8/K7[] w - - 0 1

[locust-all-points:chess]
pieceToCharTable = PNBRQK....D.....pnbrqk....d.....
customPiece1 = d:c{hurdles: 2,2; pre: 1,*; post: 1,1; capture: locust_all; hurdle_types: enemy}R
startFen = 7k/8/8/3p4/3p4/3D4/8/K7 w - - 0 1 {0 0}
pointsCounting = true
pointsRuleCaptures = us
piecePoints = p:1 d:0 k:0

[locust-all-undo:chess]
pieceToCharTable = PNBRQK....D.....pnbrqk....d.....
customPiece1 = d:c{hurdles: 2,2; pre: 1,*; post: 1,1; capture: locust_all; hurdle_types: enemy}R
startFen = 7k/8/8/3p4/3p4/3D4/8/K7 w - - 0 1

[long-step-hopper:hopper-common]
customPiece1 = d:{hurdles: 1,1; pre: 1,1; post: 1,1}(3,2)(3,2)

[parser-fail:hopper-common]
customPiece1 = d:{hurdles: abc,1; pre: 1,*}R

[parser-missing-comma:hopper-common]
customPiece1 = d:{hurdles: 2; pre: 1,*}R

[parser-unknown-hurdle-type:hopper-common]
customPiece1 = d:{hurdles: 1,1; pre: 1,*; post: 1,1; hurdle_types: enemy,bogus}R
EOF

function run_test() {
    local variant=$1
    local fen=$2
    local expected_nodes=$3
    local moves=${4:-}
    echo "Testing $variant..."
    output=$(run_uci "$ENGINE" "$INI_FILE" "$variant" <<EOF
position fen $fen $moves
go perft 1
EOF
)
    assert_nodes "$output" "$expected_nodes"
}

function expect_variant_rejected() {
    local variant=$1
    local expected_message=$2
    local output
    output=$(run_uci "$ENGINE" "$INI_FILE" "$variant" </dev/null 2>&1 || true)
    assert_contains "$output" "$expected_message" "reject variant $variant"
}

# 1. Basic Grasshopper
# White D4 (Hopper), White D5 (Hurdle). Hopper jumps to D6.
# White King A1, Black King H8.
# Moves: King A1 (3), D5D6 (1), Hopper D4D6 (1). Total = 5
run_test "hopper-base" "7k/8/8/8/3P4/3D4/8/K7 w - - 0 1" 5

# 2. Side-Symmetry (Directional atoms)
# White D4 (Hopper), White D5 (Hurdle). Forward jump to D6.
# Moves: King A1 (3), D5D6 (1), Hopper D4D6 (1). Total = 5
run_test "directional-hopper" "7k/8/8/8/3P4/3D4/8/K7 w - - 0 1" 5
# White D4 (Hopper), White D3 (Hurdle). Backward jump to D2.
# Moves: King A1 (3), D3D4? blocked. Hopper cannot jump backward.
# Total moves = 3
run_test "directional-hopper" "7k/8/8/8/8/3D4/3P4/K7 w - - 0 1" 3
# Black d5 (Hopper), Black d4 (Hurdle). Forward jump (downward) to d3.
# Moves: king h8 (3), d4d3 (1), hopper d5d3 (1). Total = 5
run_test "directional-hopper" "7k/8/3d4/3p4/8/8/8/K7 b - - 0 1" 5
# Black d5 (Hopper), Black d6 (Hurdle). Backward jump (upward) to d7.
# Moves: king h8 (3), d6d5? blocked. Hopper cannot jump backward.
# Total moves = 3
run_test "directional-hopper" "7k/3p4/3d4/8/8/8/8/K7 b - - 0 1" 3

# 3. Locust Modes
# locust_first (1 hurdle)
# White D3 (Locust), Enemy p4 (Hurdle). Jump to D5.
# King A1, King H8.
# Moves: King A1 (3), Hopper D3D5 (1). Total = 4
run_test "locust-first" "7k/8/8/8/3p4/3D4/8/K7 w - - 0 1" 4

# A king moving onto the hurdle square of a locust hop must be rejected.
# White King A1, Black hopper A2. B2 is attacked because the hopper lands on C2.
run_test "locust-king" "7k/8/8/8/8/8/d7/K7 w - - 0 1" 2

# Verify capture happened
output=$(run_uci "$ENGINE" "$INI_FILE" locust-first << 'EOF'
position fen 7k/8/8/8/3p4/3D4/8/K7 w - - 0 1 moves d3d5
d
EOF
)
assert_contains "$output" "Fen: 7k/8/8/3D4/8/8/8/K7" "locust_first captured hurdle"

# Transparent piece types should be ignored by the hopper ray while hurdle_piece_types
# still count as the captured hurdle.
# White D3 (hopper), white p4 (transparent), black n5 (hurdle). D3D6 should be generated.
run_test "piece-type-hurdles" "7k/8/8/3n4/3p4/3D4/8/K7 w - - 0 1" 4
output=$(run_uci "$ENGINE" "$INI_FILE" piece-type-hurdles << 'EOF'
position fen 7k/8/8/3n4/3p4/3D4/8/K7 w - - 0 1 moves d3d6
d
EOF
)
assert_contains "$output" "Fen: 7k/8/3D4/8/3p4/8/8/K7" "piece-type hurdle/transparent parsing works"

# locust_all (Kangaroo)
# White D3, Enemy p4, p5. Jump to D6.
# Moves: King A1 (3), Hopper D3D6 (1). Total = 4
run_test "locust-all" "7k/8/8/3p4/3p4/3D4/8/K7 w - - 0 1" 4
output=$(run_uci "$ENGINE" "$INI_FILE" locust-all << 'EOF'
position fen 7k/8/8/3p4/3p4/3D4/8/K7 w - - 0 1 moves d3d6
d
EOF
)
assert_contains "$output" "Fen: 7k/8/3D4/8/8/8/8/K7" "locust_all captured multiple hurdles"

# locust_all captures every crossed hurdle, so with selfCapture disabled it
# must reject lines that include any friendly hurdle.
# White D3, enemy on D4, friendly on D5. D3D6 must be rejected.
# Moves: king A1 (3), friendly D5D6 (1) => 4.
run_test "locust-all-friendly-mix" "7k/8/8/3P4/3p4/3D4/8/K7 w - - 0 1" 4

# Friendly hurdles must not be capturable unless self-capture is enabled.
# White D3 and friendly hurdle D4. Locust jump D3D5 must be rejected.
# Moves: king A1 (3), friendly hurdle push D4D5 (1) => 4.
run_test "locust-friendly-hurdle" "7k/8/8/8/3P4/3D4/8/K7 w - - 0 1" 4

# With selfCapture enabled, friendly locust hurdle capture is legal.
# White D3 and friendly hurdle D4. D3D5 should now be available.
# Moves: king A1 (3), friendly hurdle push D4D5 (1), hopper D3D5 (1) => 5.
run_test "locust-friendly-selfcapture" "7k/8/8/8/3P4/3D4/8/K7 w - - 0 1" 5

# locust_last captures the last hurdle when multiple hurdles are crossed.
run_test "locust-last" "7k/8/8/3p4/3p4/3D4/8/K7 w - - 0 1" 4
output=$(run_uci "$ENGINE" "$INI_FILE" locust-last << 'EOF'
position fen 7k/8/8/3p4/3p4/3D4/8/K7 w - - 0 1 moves d3d6
d
EOF
)
# Last hurdle (d5) should be removed, first hurdle (d4) should remain.
assert_contains "$output" "Fen: 7k/8/3D4/8/3p4/8/8/K7" "locust_last captured only the last hurdle"

# CAPTURE_DEST with enemy hurdle + enemy destination:
# D at d3, enemies at d4 and d5. d3d5 must be generated as a capture.
output=$(run_uci "$ENGINE" "$INI_FILE" dest-capture-hopper << 'EOF'
position fen 7k/8/8/3p4/3p4/3D4/8/K7 w - - 0 1
go perft 1
EOF
)
assert_contains "$output" "^d3d5: 1$" "destination-capture hopper can hop over enemy hurdle"

# CAPTURE_DEST must not capture directly without first crossing a hurdle.
output=$(run_uci "$ENGINE" "$INI_FILE" dest-capture-no-hurdle << 'EOF'
position fen 7k/8/8/8/3p4/3D4/8/K7 w - - 0 1
go perft 1
EOF
)
assert_not_contains "$output" "^d3d4: 1$" "destination-capture hopper illegally captured without hurdle"

# 4. Equi-family
# Equihopper (pre=1, post=1)
# White D3, White D4 (Hurdle). Jump to D5. King A1.
# Moves: King A1 (3), D4D5 (1), Hopper D3D5 (1) = 5
run_test "equi-hopper" "7k/8/8/8/3P4/3D4/8/K7 w - - 0 1" 5
# Equihopper (pre=2, post=2)
# White D3, White D5 (Hurdle). Jump to D7. King A1.
# Moves: King A1 (3), Hopper D3D7 (1). Total = 4
run_test "equi-hopper" "7k/8/3P4/8/3D4/8/8/K7 w - - 0 1" 4
# Equistopper (halfway to hurdle)
# D at D1, p at D5. Stopper should land at D3. King H1.
# White King at H1: G1, G2, H2 (3 moves).
# White Stopper D1: jumps halfway to p5 (D5) -> D3 (1 move).
# White Stopper D1: jumps halfway to King H1 (H1) -> F1 (1 move).
# Total = 5.
run_test "equi-stopper" "7k/8/8/3p4/8/8/8/3D3K w - - 0 1" 5

# Equistopper-multi (halfway to 2nd hurdle)
# D at D1, p at D3 (1st), p at D5 (2nd). King H1.
# Stopper halfway to 2nd hurdle (D5) lands at D3.
# D3 is occupied by 1st hurdle, so D1D3 is a capture move.
# Moves: King H1 (3), Stopper D1D3 (1) = 4.
run_test "equi-stopper-multi" "7k/8/8/3p4/8/3p4/8/3D3K w - - 0 1" 4

# Equistopper with pre-distance constraint:
# D at D3, hurdle at D5, but pre is constrained to 3.
# Midpoint move D3D4 (which requires pre=2) must be rejected.
# Moves: King A1 only (3). Total = 3.
run_test "equi-stopper-pre3" "7k/8/8/3p4/8/3D4/8/K7 w - - 0 1" 3

# 5. Wrapped topology
# Rook-hopper on cylinder jumping across the edge
# D on h4, P on a4. Should land on b4.
# King on A1, Black King H8.
# Moves: Hopper H4B4 (1), King A1 (3). Total = 4
run_test "wrapped-hopper" "7k/8/8/8/P6D/8/8/K7 w - - 0 1" 4

# 5b. Long-step tuple hopper (3,2) ray should not be blocked by anti-wrap guards.
# D on a1, hurdle on d3, landing on g5. Plus two pawn pushes and king moves.
# Moves: D a1g5 (1), d3d4 (1), g6g7 (1), king h1 (3). Total = 6
run_test "long-step-hopper" "7k/8/6P1/8/8/3P4/8/D6K w - - 0 1" 6

# 5c. Wrapped topology + locust_all: capture-all should still remove all hurdles.
# D at h3, enemy hurdles at h4/h5, landing at h6.
run_test "wrapped-locust-all" "7k/8/8/p6p/p6p/7D/8/K7 w - - 0 1" 4
output=$(run_uci "$ENGINE" "$INI_FILE" wrapped-locust-all << 'EOF'
position fen 7k/8/8/p6p/p6p/7D/8/K7 w - - 0 1 moves h3h6
d
EOF
)
assert_contains "$output" "Fen: 7k/8/7D/p7/p7/8/8/K7" "wrapped locust_all removed all hurdles"

# 5ca. Wrapped topology + initial universal hopper captures should be generated too.
output=$(run_uci "$ENGINE" "$INI_FILE" wrapped-initial-locust << 'EOF'
position fen 8/8/8/8/8/4p3/4A3/K6k w - - 0 1
go perft 1
EOF
)
assert_contains "$output" "^e2e4: 1$" "wrapped initial locust capture is generated"
assert_nodes "$output" 4

# 5d. locust_all side effects: transfer all captured hurdles to hand.
output=$(run_uci "$ENGINE" "$INI_FILE" locust-all-hand << 'EOF'
position startpos moves d3d6
d
EOF
)
assert_contains "$output" "Fen: 7k/8/3D4/8/8/8/8/K7\\[PP\\]" "locust_all transfers all captured hurdles to hand"

# 5e. locust_all side effects: award points for all captured hurdles.
output=$(run_uci "$ENGINE" "$INI_FILE" locust-all-points << 'EOF'
position startpos moves d3d6
d
EOF
)
assert_contains "$output" "Fen: 7k/8/3D4/8/8/8/8/K7 b - - 0 1 {2 0}" "locust_all awards points for all captured hurdles"

# 5f. do/undo integrity for locust_all multi-capture:
# perft(2) must be stable across repeated runs, and root FEN must remain unchanged.
output=$(run_uci "$ENGINE" "$INI_FILE" locust-all-undo << 'EOF'
position startpos
go perft 2
go perft 2
d
EOF
)
nodes=($(echo "$output" | grep "Nodes searched:" | awk '{print $3}'))
if [[ "${#nodes[@]}" -lt 2 || "${nodes[0]}" != "${nodes[1]}" ]]; then
    echo "  [FAIL] locust_all perft(2) instability suggests do/undo corruption"
    echo "Output was:"
    echo "$output"
    exit 1
fi
assert_contains "$output" "Fen: 7k/8/8/3p4/3p4/3D4/8/K7 w - - 0 1" "locust_all preserves root state across repeated perft"

# 6. Parser robustness
# Malformed numeric hopper ranges are now rejected during variant loading.
expect_variant_rejected "parser-fail" "unknown variant 'parser-fail'; keeping 'chess'"
expect_variant_rejected "parser-missing-comma" "unknown variant 'parser-missing-comma'; keeping 'chess'"

reject_out=$(run_uci "$ENGINE" "$INI_FILE" parser-unknown-hurdle-type </dev/null 2>&1 || true)
assert_contains "$reject_out" "Unknown Betza hopper special type 'bogus'" "reject unknown hurdle type"
run_test "parser-unknown-hurdle-type" "7k/8/8/8/3P4/3D4/8/K7 w - - 0 1" 4

echo "All Universal Hopper tests passed!"
