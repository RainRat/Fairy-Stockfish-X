#!/usr/bin/env bash

set -euo pipefail

# Test Universal Hopper features

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
    output=$(run_uci "${ENGINE}" "${INI_FILE}" "$variant" <<EOF
position fen $fen $moves
go perft 1
EOF
)
    assert_nodes "$output" "$expected_nodes"
    echo "  [PASS] Nodes: $expected_nodes"
}

function expect_variant_rejected() {
    local variant=$1
    local expected_message=$2
    echo "Testing rejection of $variant..."
    output=$(uci_timeout "${ENGINE}" << EOF 2>&1
uci
setoption name VariantPath value $INI_FILE
setoption name UCI_Variant value $variant
quit
EOF
)
    assert_contains "$output" "$expected_message" "be rejected with message"
    echo "  [PASS] Rejected as expected"
}

# 1. Basic Grasshopper
run_test "hopper-base" "7k/8/8/8/3P4/3D4/8/K7 w - - 0 1" 5

# 2. Side-Symmetry (Directional atoms)
run_test "directional-hopper" "7k/8/8/8/3P4/3D4/8/K7 w - - 0 1" 5
run_test "directional-hopper" "7k/8/8/8/8/3D4/3P4/K7 w - - 0 1" 3
run_test "directional-hopper" "7k/8/3d4/3p4/8/8/8/K7 b - - 0 1" 5
run_test "directional-hopper" "7k/3p4/3d4/8/8/8/8/K7 b - - 0 1" 3

# 3. Locust Modes
run_test "locust-first" "7k/8/8/8/3p4/3D4/8/K7 w - - 0 1" 4
run_test "locust-king" "7k/8/8/8/8/8/d7/K7 w - - 0 1" 2

# Verify capture happened
output=$(run_uci "${ENGINE}" "${INI_FILE}" "locust-first" <<'UCI'
position fen 7k/8/8/8/3p4/3D4/8/K7 w - - 0 1 moves d3d5
d
UCI
)
assert_fen "$output" "7k/8/8/3D4/8/8/8/K7 b - - 0 1"
echo "  [PASS] locust_first captured hurdle"

# Transparent piece types
run_test "piece-type-hurdles" "7k/8/8/3n4/3p4/3D4/8/K7 w - - 0 1" 4
output=$(run_uci "${ENGINE}" "${INI_FILE}" "piece-type-hurdles" <<'UCI'
position fen 7k/8/8/3n4/3p4/3D4/8/K7 w - - 0 1 moves d3d6
d
UCI
)
assert_fen "$output" "7k/8/3D4/8/3p4/8/8/K7 b - - 0 1"
echo "  [PASS] piece-type hurdle/transparent parsing works"

# locust_all (Kangaroo)
run_test "locust-all" "7k/8/8/3p4/3p4/3D4/8/K7 w - - 0 1" 4
output=$(run_uci "${ENGINE}" "${INI_FILE}" "locust-all" <<'UCI'
position fen 7k/8/8/3p4/3p4/3D4/8/K7 w - - 0 1 moves d3d6
d
UCI
)
assert_fen "$output" "7k/8/3D4/8/8/8/8/K7 b - - 0 1"
echo "  [PASS] locust_all captured multiple hurdles"

run_test "locust-all-friendly-mix" "7k/8/8/3P4/3p4/3D4/8/K7 w - - 0 1" 4
run_test "locust-friendly-hurdle" "7k/8/8/8/3P4/3D4/8/K7 w - - 0 1" 4
run_test "locust-friendly-selfcapture" "7k/8/8/8/3P4/3D4/8/K7 w - - 0 1" 5

# locust_last
run_test "locust-last" "7k/8/8/3p4/3p4/3D4/8/K7 w - - 0 1" 4
output=$(run_uci "${ENGINE}" "${INI_FILE}" "locust-last" <<'UCI'
position fen 7k/8/8/3p4/3p4/3D4/8/K7 w - - 0 1 moves d3d6
d
UCI
)
# Last hurdle (d5) should be removed, first hurdle (d4) should remain.
assert_fen "$output" "7k/8/3D4/8/3p4/8/8/K7 b - - 0 1"
echo "  [PASS] locust_last captured only the last hurdle"

# CAPTURE_DEST with enemy hurdle + enemy destination:
output=$(run_uci "${ENGINE}" "${INI_FILE}" "dest-capture-hopper" <<'UCI'
position fen 7k/8/8/3p4/3p4/3D4/8/K7 w - - 0 1
go perft 1
UCI
)
assert_contains "$output" "^d3d5: 1$"
echo "  [PASS] destination-capture hopper can hop over enemy hurdle"

# CAPTURE_DEST must not capture directly without first crossing a hurdle.
output=$(run_uci "${ENGINE}" "${INI_FILE}" "dest-capture-no-hurdle" <<'UCI'
position fen 7k/8/8/8/3p4/3D4/8/K7 w - - 0 1
go perft 1
UCI
)
assert_not_contains "$output" "^d3d4: 1$"
echo "  [PASS] destination-capture requires crossing a hurdle first"

# 4. Equi-family
run_test "equi-hopper" "7k/8/8/8/3P4/3D4/8/K7 w - - 0 1" 5
run_test "equi-hopper" "7k/8/3P4/8/3D4/8/8/K7 w - - 0 1" 4
run_test "equi-stopper" "7k/8/8/3p4/8/8/8/3D3K w - - 0 1" 5
run_test "equi-stopper-multi" "7k/8/8/3p4/8/3p4/8/3D3K w - - 0 1" 4
run_test "equi-stopper-pre3" "7k/8/8/3p4/8/3D4/8/K7 w - - 0 1" 3

# 5. Wrapped topology
run_test "wrapped-hopper" "7k/8/8/8/P6D/8/8/K7 w - - 0 1" 4
run_test "long-step-hopper" "7k/8/6P1/8/8/3P4/8/D6K w - - 0 1" 6
run_test "wrapped-locust-all" "7k/8/8/p6p/p6p/7D/8/K7 w - - 0 1" 4
output=$(run_uci "${ENGINE}" "${INI_FILE}" "wrapped-locust-all" <<'UCI'
position fen 7k/8/8/p6p/p6p/7D/8/K7 w - - 0 1 moves h3h6
d
UCI
)
assert_fen "$output" "7k/8/7D/p7/p7/8/8/K7 b - - 0 1"
echo "  [PASS] wrapped locust_all removed all hurdles"

output=$(run_uci "${ENGINE}" "${INI_FILE}" "wrapped-initial-locust" <<'UCI'
position fen 8/8/8/8/8/4p3/4A3/K6k w - - 0 1
go perft 1
UCI
)
assert_contains "$output" "^e2e4: 1$"
assert_nodes "$output" 4
echo "  [PASS] wrapped initial locust capture is generated"

# 5d. locust_all side effects: transfer all captured hurdles to hand.
output=$(run_uci "${ENGINE}" "${INI_FILE}" "locust-all-hand" <<'UCI'
position startpos moves d3d6
d
UCI
)
assert_contains "$output" "Fen: 7k/8/3D4/8/8/8/8/K7\\[PP\\]"
echo "  [PASS] locust_all transfers all captured hurdles to hand"

# 5e. locust_all side effects: award points for all captured hurdles.
output=$(run_uci "${ENGINE}" "${INI_FILE}" "locust-all-points" <<'UCI'
position startpos moves d3d6
d
UCI
)
assert_fen "$output" "7k/8/3D4/8/8/8/8/K7 b - - 0 1 {2 0}"
echo "  [PASS] locust_all awards points for all captured hurdles"

# 5f. do/undo integrity for locust_all multi-capture:
output=$(run_uci "${ENGINE}" "${INI_FILE}" "locust-all-undo" <<'UCI'
position startpos
go perft 2
go perft 2
d
UCI
)
nodes=($(echo "$output" | grep "Nodes searched:" | awk '{print $3}'))
if [[ "${#nodes[@]}" -lt 2 || "${nodes[0]}" != "${nodes[1]}" ]]; then
    echo "  [FAIL] locust_all perft(2) instability suggests do/undo corruption"
    echo "Output was:"
    echo "$output"
    exit 1
fi
assert_fen "$output" "7k/8/8/3p4/3p4/3D4/8/K7 w - - 0 1"
echo "  [PASS] locust_all preserves root state across repeated perft"

# 6. Parser robustness
expect_variant_rejected "parser-fail" "unknown variant 'parser-fail'; keeping 'chess'"
expect_variant_rejected "parser-missing-comma" "unknown variant 'parser-missing-comma'; keeping 'chess'"

output=$(uci_timeout "${ENGINE}" << EOF 2>&1
uci
setoption name VariantPath value $INI_FILE
setoption name UCI_Variant value parser-unknown-hurdle-type
quit
EOF
)
assert_contains "$output" "Unknown Betza hopper special type 'bogus'"
run_test "parser-unknown-hurdle-type" "7k/8/8/8/3P4/3D4/8/K7 w - - 0 1" 4

echo "All Universal Hopper tests passed!"
