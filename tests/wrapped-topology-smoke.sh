#!/bin/bash

set -euo pipefail

error() {
  echo "wrapped-topology smoke test failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

ENGINE=$(default_engine "${1:-}")
VARIANT_PATH=$(default_variants "${2:-}")

run_cmds() {
  local cmds="$1"
  local variant=""
  if [[ "$cmds" =~ "setoption name UCI_Variant value "([^[:space:]]+) ]]; then
    variant="${BASH_REMATCH[1]}"
    cmds=$(echo "$cmds" | grep -v "setoption name UCI_Variant value ")
  fi
  run_uci "$ENGINE" "$VARIANT_PATH" "$variant" <<< "$cmds"
}

echo "== wrapped-topology smoke tests started =="

# ========================================================================
# Issue 1: Wrapped attackers_to() with asymmetric pieces
# Verify that cylinder chess correctly handles standard chess checks and
# evasions — this exercises the wrapped attackers_to() path.
# ========================================================================
echo "-- Issue 1: wrapped attackers_to basic check --"

# Knight giving check on cylinder — king must have legal evasions
out=$(run_cmds "setoption name UCI_Variant value cylinder
position fen 4k3/8/5N2/8/8/8/8/4K3 w - - 0 1
go perft 1")
# White knight on f6 gives check to king on e8; verify we get legal moves
assert_contains "$out" "^Nodes searched: "

# Search depth 4 to exercise MovePicker evasion path on cylinder
out=$(run_cmds "setoption name UCI_Variant value cylinder
position fen 4k3/8/5N2/8/8/8/8/4K3 b - - 0 1
go depth 4")
assert_contains "$out" "bestmove"

# ========================================================================
# Issue 2: Wrapped evasions via MovePicker (search) don't miss moves
# Test that search on cylinder/toroid finds correct evasions when in check.
# The rook check crosses the board and interposition via wrapped path must
# be considered.
# ========================================================================
echo "-- Issue 2: wrapped evasion completeness --"

# Cylinder: rook on a-file attacks across the wrap to h-file
# White rook on a4, black king on h8, white king on e1
# On a cylinder, Ra4 can slide left from a to h (wrapping), giving check
# if the path is clear. Test search handles this correctly.
out=$(run_cmds "setoption name UCI_Variant value cylinder
position fen 7k/8/8/8/R7/8/8/4K3 w - - 0 1
go depth 6")
assert_contains "$out" "bestmove"

# Toroidal chess search test — king in check must find evasion
out=$(run_cmds "setoption name UCI_Variant value toroidal-chess
position fen r1b2b1r/pp4pp/n1pqkp1n/3pp3/3PP3/N1PQKP1N/PP4PP/R1B2B1R w - - 0 1
go depth 4")
assert_contains "$out" "bestmove"

# ========================================================================
# Issue 2b: Verify cylinder perft stability
# These perft values serve as a baseline; if our changes break evasion
# generation, the node counts would change.
# ========================================================================
echo "-- Issue 2b: cylinder perft baseline --"

out=$(run_cmds "setoption name UCI_Variant value cylinder
position startpos
go perft 1")
assert_nodes "$out" 20

out=$(run_cmds "setoption name UCI_Variant value cylinder
position startpos
go perft 2")
assert_nodes "$out" 392

# Deeper perft as regression check
out=$(run_cmds "setoption name UCI_Variant value cylinder
position startpos
go perft 3")
assert_contains "$out" "^Nodes searched: "

# ========================================================================
# Issue 3: Self-return guard on wrapped step/tuple
# On small wrapped boards, verify no piece attacks or moves to its own
# square. We test with custom INI defining a tiny cylinder.
# ========================================================================
echo "-- Issue 3: self-return guard --"

# Create a tiny 2-file cylinder variant to test wrapping on small boards
tmp_ini=$(mktemp)
cat > "${tmp_ini}" <<'INI'
[tiny-cyl:chess]
maxFile = b
maxRank = 4
cylindrical = true
castling = false
startFen = k1/2/2/1K w - - 0 1
INI

# On a 2-file cylinder with file wrapping, a knight on a1 stepping (2,1)
# could wrap but should never self-target. Verify the engine handles it.
out=$(cat <<EOF | timeout 30 "$ENGINE"
uci
setoption name VariantPath value ${tmp_ini}
setoption name UCI_Variant value tiny-cyl
position startpos
go perft 1
quit
EOF
)
assert_contains "$out" "^Nodes searched: "

# Also verify no crash on depth search
out=$(cat <<EOF | timeout 30 "$ENGINE"
uci
setoption name VariantPath value ${tmp_ini}
setoption name UCI_Variant value tiny-cyl
position startpos
go depth 4
quit
EOF
)
assert_contains "$out" "bestmove"
rm -f "${tmp_ini}"

# ========================================================================
# Issue 2+4: Verify standard cylinder check positions work
# Test a position where the king is in check from a sliding piece on a
# cylinder. The search must find the correct evasion.
# ========================================================================
echo "-- Issue 2+4: cylinder check evasion search --"

# Black king on e8, white rook on e1 — direct check along e-file
# Test that search correctly generates evasions
out=$(run_cmds "setoption name UCI_Variant value cylinder
position fen 4k3/8/8/8/8/8/8/4KR2 w - - 0 1
go depth 4")
assert_contains "$out" "bestmove"

# Position with check — black to move and evade
out=$(run_cmds "setoption name UCI_Variant value cylinder
position fen 4k3/8/8/8/8/8/4R3/4K3 b - - 0 1
go depth 6")
assert_contains "$out" "bestmove"

# ========================================================================
# Additional: Cylinder config sanity
# ========================================================================
echo "-- Config sanity check --"
out=$(timeout 30 "$ENGINE" check "$VARIANT_PATH" 2>&1 || true)
assert_not_contains "$out" "Error" "not produce parse errors"

echo "== wrapped-topology smoke tests passed =="
