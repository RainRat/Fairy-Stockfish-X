#!/usr/bin/env bash

set -euo pipefail

# Test Universal Hopper features

# 1. Setup temporary variant file
ENGINE=${1:-./stockfish}
if [[ ! -x "${ENGINE}" ]]; then
    ENGINE="../src/stockfish"
fi
if [[ ! -x "${ENGINE}" ]]; then
    echo "engine executable not found: pass path as first argument" >&2
    exit 2
fi

INI_FILE=$(mktemp -t universal_hopper_test.XXXXXX.ini)
trap 'rm -f "${INI_FILE}"' EXIT
cat << 'EOF' > $INI_FILE
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

[locust-all:hopper-common]
customPiece1 = d:{hurdles: 2,2; pre: 1,*; post: 1,1; capture: locust_all}R

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

[wrapped-locust-all:hopper-common]
topology = cylinder
customPiece1 = d:c{hurdles: 2,2; pre: 1,1; post: 1,1; capture: locust_all; hurdle_types: enemy}R

[long-step-hopper:hopper-common]
customPiece1 = d:{hurdles: 1,1; pre: 1,1; post: 1,1}(3,2)(3,2)

[parser-fail:hopper-common]
customPiece1 = d:{hurdles: abc,1; pre: 1,*}R

[parser-missing-comma:hopper-common]
customPiece1 = d:{hurdles: 2; pre: 1,*}R
EOF

function run_test() {
    local variant=$1
    local fen=$2
    local expected_nodes=$3
    local moves=${4:-}
    echo "Testing $variant..."
output=$("${ENGINE}" << EOF
uci
setoption name VariantPath value $INI_FILE
setoption name UCI_Variant value $variant
position fen $fen $moves
go perft 1
quit
EOF
)
    nodes=$(echo "$output" | grep "Nodes searched:" | awk '{print $3}')
    if [ "$nodes" -eq "$expected_nodes" ]; then
        echo "  [PASS] Nodes: $nodes"
    else
        echo "  [FAIL] Expected $expected_nodes, got $nodes"
        echo "Output was:"
        echo "$output"
        exit 1
    fi
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
# Verify capture happened
output=$("${ENGINE}" << EOF
uci
setoption name VariantPath value $INI_FILE
setoption name UCI_Variant value locust-first
position fen 7k/8/8/8/3p4/3D4/8/K7 w - - 0 1 moves d3d5
d
quit
EOF
)
if echo "$output" | grep -q "Fen: 7k/8/8/3D4/8/8/8/K7"; then
    echo "  [PASS] locust_first captured hurdle"
else
    echo "  [FAIL] locust_first did not capture hurdle"
    echo "Output was:"
    echo "$output"
    exit 1
fi

# locust_all (Kangaroo)
# White D3, Enemy p4, p5. Jump to D6.
# Moves: King A1 (3), Hopper D3D6 (1). Total = 4
run_test "locust-all" "7k/8/8/3p4/3p4/3D4/8/K7 w - - 0 1" 4
output=$("${ENGINE}" << EOF
uci
setoption name VariantPath value $INI_FILE
setoption name UCI_Variant value locust-all
position fen 7k/8/8/3p4/3p4/3D4/8/K7 w - - 0 1 moves d3d6
d
quit
EOF
)
if echo "$output" | grep -q "Fen: 7k/8/3D4/8/8/8/8/K7"; then
    echo "  [PASS] locust_all captured multiple hurdles"
else
    echo "  [FAIL] locust_all did not capture all hurdles"
    exit 1
fi

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
output=$("${ENGINE}" << EOF
uci
setoption name VariantPath value $INI_FILE
setoption name UCI_Variant value wrapped-locust-all
position fen 7k/8/8/p6p/p6p/7D/8/K7 w - - 0 1 moves h3h6
d
quit
EOF
)
if echo "$output" | grep -q "Fen: 7k/8/7D/p7/p7/8/8/K7"; then
    echo "  [PASS] wrapped locust_all removed all hurdles"
else
    echo "  [FAIL] wrapped locust_all did not remove all hurdles"
    exit 1
fi

# 6. Parser Robustness
# Should not crash and use default (1) for 'abc'
# customPiece1 = d:{hurdles: abc,1; pre: 1,*}R
# White D3, White D4 (Hurdle). Jump to D5. King A1.
# Moves: King A1 (3), D4D5 (1), Hopper D3D5 (1). Total = 5
run_test "parser-fail" "7k/8/8/8/3P4/3D4/8/K7 w - - 0 1" 5

# Missing comma in hurdles parameter should not crash and should warn.
# customPiece1 = d:{hurdles: 2; pre: 1,*}R defaults to hurdles 1,1.
# White D3, White D4 (Hurdle). Jump to D5. King A1.
# Moves: King A1 (3), D4D5 (1), Hopper D3D5 (1). Total = 5
run_test "parser-missing-comma" "7k/8/8/8/3P4/3D4/8/K7 w - - 0 1" 5

echo "All Universal Hopper tests passed!"
