#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ROOT_DIR=$(cd "${SCRIPT_DIR}/.." && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

ENGINE="${1:-${ROOT_DIR}/src/stockfish-vlb}"
VARIANTS="${2:-${ROOT_DIR}/src/variants.ini}"
setup_test_context "${ENGINE}" "${VARIANTS}" "vlb regressions"

if [[ ! -x "${ENGINE}" ]]; then
  echo "skip: VLB engine not available"
  exit 0
fi

test_vlb_gale_smoke() {
  local output
  output=$(run_uci "$ENGINE" "$VARIANTS" gale <<'EOF' 2>&1
position startpos
go perft 1
EOF
)

  if grep -q "variants skipped because of board size limits" <<<"$output"; then
    echo "skip: gale requires VERY_LARGE_BOARDS"
    return 0
  fi

  if grep -q "No such variant: gale" <<<"$output"; then
    echo "skip: gale unavailable in this binary"
    return 0
  fi

  assert_contains_literal "$output" "info string variant gale "
  assert_contains_literal "$output" "Nodes searched: 41"
}

test_vlb_lame_riders() {
  local tmpdir variant_file out
  tmpdir=$(mktemp -d)
  trap 'rm -rf "$tmpdir"' RETURN

  variant_file="$tmpdir/vlb-lame-riders.ini"
  cat > "$variant_file" <<'VAR'
[vlb-lame-clear:fairy]
maxFile = p
maxRank = 16
customPiece1 = a:n{path:mid}D
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 12A2k/16/16/16/16/16/16/16/16/16/16/16/16/16/16/15K w - - 0 1

[vlb-lame-blocked:fairy]
maxFile = p
maxRank = 16
customPiece1 = a:n{path:mid}D
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 12A2k/12p3/16/16/16/16/16/16/16/16/16/16/16/16/16/15K w - - 0 1
VAR

  out=$(run_uci "$ENGINE" "$variant_file" vlb-lame-clear <<'UCI' 2>&1
position startpos
go perft 1
UCI
)
  if grep -q "variants skipped because of board size limits" <<<"$out"; then
    echo "skip: VLB lame rider regression requires a very-large-board capable engine"
    return 0
  fi
  if grep -q "No such variant" <<<"$out"; then
    echo "skip: VLB lame rider variant unavailable in this binary"
    return 0
  fi

  assert_contains "$out" "info string variant vlb-lame-clear files 16 ranks 16"
  assert_contains "$out" "^m16m14: 1$"
  assert_contains "$out" "^m16k16: 1$"
  assert_contains "$out" "^m16o16: 1$"
  assert_nodes "$out" 6

  out=$(run_uci "$ENGINE" "$variant_file" vlb-lame-blocked <<'UCI' 2>&1
position startpos
go perft 1
UCI
)
  assert_contains "$out" "info string variant vlb-lame-blocked files 16 ranks 16"
  assert_not_contains "$out" "^m16m14:"
  assert_contains "$out" "^m16k16: 1$"
  assert_contains "$out" "^m16o16: 1$"
  assert_nodes "$out" 5
}

test_vlb_symbol_check() {
  local tmpdir variant_file out
  tmpdir=$(mktemp -d)
  trap 'rm -rf "$tmpdir"' RETURN

  variant_file="$tmpdir/vlb-symbol-check.ini"
  cat > "$variant_file" <<'VAR'
[vlb-token-check:fairy]
maxRank = 5
maxFile = 5
customPiece1 = a':W
customPiece2 = a":F
startFen = 4k/5/5/5/A'2A"K w - - 0 1
VAR

  out=$(
    printf 'setoption name VariantPath value %s\nquit\n' "$variant_file" \
      | "$ENGINE" 2>&1
  )

  printf '%s\n' "$out"
  [[ "$out" != *"Ambiguous piece character"* ]]
  [[ "$out" != *"Ambiguous piece symbol"* ]]
}

test_vlb_symbol_fen() {
  local tmpdir variant_file out
  tmpdir=$(mktemp -d)
  trap 'rm -rf "$tmpdir"' RETURN

  variant_file="$tmpdir/vlb-symbol.ini"
  cat > "$variant_file" <<'VAR'
[vlb-token-smoke:fairy]
maxRank = 5
maxFile = 5
pieceDrops = true
customPiece1 = a':W
pieceValueMg = a':321
startFen = 4k/5/5/5/A'3K[A'a'] w - - 0 1
VAR

  out=$(
    printf 'setoption name VariantPath value %s\nsetoption name UCI_Variant value vlb-token-smoke\nposition startpos\nd\ngo perft 1\nquit\n' "$variant_file" \
      | "$ENGINE" 2>&1
  )

  printf '%s\n' "$out"
  [[ "$out" == *"variant vlb-token-smoke"* ]]
  [[ "$out" == *"Fen: 4k/5/5/5/A'3K[A'a'] w - - 0 1"* ]]
  [[ "$out" == *" |A' |"* ]]
  [[ "$out" == *"A'@b1: 1"* ]]
  [[ "$out" == *"Nodes searched: 27"* ]]
  [[ "$out" != *"Invalid syntax"* ]]
  [[ "$out" != *"Invalid piece character"* ]]
}

test_vlb_symbol_options() {
  local tmpdir variant_file out
  tmpdir=$(mktemp -d)
  trap 'rm -rf "$tmpdir"' RETURN

  variant_file="$tmpdir/vlb-symbol-options.ini"
  cat > "$variant_file" <<'VAR'
[vlb-token-options:fairy]
maxRank = 5
maxFile = 5
pieceDrops = true
customPiece1 = z':W
customPiece2 = y':F
pieceValueMg = z':321 y':222
pieceValueEg = z':111 y':112
piecePoints = z':3 y':4
promotionLimit = z':1 y':2
promotedPieceType = p:z' z':q
moveMorphPieceType = n:z' y':-
dropPieceTypes = k:z' y'; z':-; y':z'
priorityDropTypes = z' y'
pushingStrength = z':2 y':3
virtualDropLimit = z':2 y':1
captureForbidden = z':y'
connectN = 3
connectPieceTypes = z' y'
connectGoalByType = true
connectPieceGoalWhite = z' y'
connectPieceGoalBlack = y' z'
startFen = 4k/5/5/5/Z'NY'1K[Z'Y'] w - - 0 1
VAR

  out=$(
    printf 'setoption name VariantPath value %s\nsetoption name UCI_Variant value vlb-token-options\nposition startpos\nd\ngo perft 1\nquit\n' "$variant_file" \
      | "$ENGINE" 2>&1
  )

  printf '%s\n' "$out"
  [[ "$out" == *"variant vlb-token-options"* ]]
  [[ "$out" == *"Fen: 4k/5/5/5/Z'NY'1K[Y'Z'] w - - 0 1"* ]]
  [[ "$out" == *"Z'"* ]]
  [[ "$out" == *"Y'"* ]]
  [[ "$out" == *"Nodes searched:"* ]]
  [[ "$out" != *"Invalid syntax"* ]]
  [[ "$out" != *"Invalid piece type"* ]]
}

test_vlb_toot_otto_drops() {
  local output
  output=$(run_uci "$ENGINE" "$VARIANTS" toot-otto <<'EOF' 2>&1
position startpos
go perft 1
EOF
)

  assert_contains_literal "$output" "info string variant toot-otto "
  assert_contains "$output" "^T@a1: 1$"
  assert_contains "$output" "^O@a1: 1$"
  assert_not_contains "$output" "^P@"
  assert_nodes "$output" 12
}

test_vlb_overlap_parity() {
  local normal_engine="${NORMAL_ENGINE:-${ROOT_DIR}/src/stockfish}"
  if [[ ! -x "${normal_engine}" ]]; then
    echo "skip: normal engine unavailable for VLB overlap parity"
    return 0
  fi

  local tmpdir variant_file variant normal_out vlb_out normal_moves vlb_moves expected_from
  tmpdir=$(mktemp -d)
  trap 'rm -rf "$tmpdir"' RETURN
  variant_file="$tmpdir/vlb-overlap-parity.ini"
  cat > "$variant_file" <<'VAR'
[parity-universal-hopper:chess]
pieceToCharTable = PNBRQKDFGHS
customPiece1 = d:{hurdles: 1,1; pre: 1,*; post: 1,1}Q
startFen = 7k/8/8/8/3p4/3D4/8/K7 w - - 0 1

[parity-griffon:chess]
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
customPiece1 = a:O
startFen = 7k/8/8/8/3A4/8/8/K7 w - - 0 1

[parity-manticore:chess]
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
customPiece1 = a:M
startFen = 7k/8/8/8/3A4/8/8/K7 w - - 0 1
VAR

  for variant in parity-universal-hopper parity-griffon parity-manticore; do
    normal_out=$(run_uci "$normal_engine" "$variant_file" "$variant" <<'UCI' 2>&1
position startpos
go perft 1
UCI
)
    vlb_out=$(run_uci "$ENGINE" "$variant_file" "$variant" <<'UCI' 2>&1
position startpos
go perft 1
UCI
)
    assert_contains_literal "$normal_out" "info string variant ${variant} "
    assert_contains_literal "$vlb_out" "info string variant ${variant} "

    expected_from=$([[ "$variant" == parity-universal-hopper ]] && printf 'd3' || printf 'd4')
    if ! grep -q "^${expected_from}[[:alnum:]]*: [0-9][0-9]*$" <<<"$normal_out"; then
      echo "normal overlap test did not exercise ${variant}" >&2
      return 1
    fi
    if ! grep -q "^${expected_from}[[:alnum:]]*: [0-9][0-9]*$" <<<"$vlb_out"; then
      echo "VLB overlap test did not exercise ${variant}" >&2
      return 1
    fi

    normal_moves=$(awk '/^[[:alnum:]@]+: [0-9]+$/ { print }' <<<"$normal_out" | sort)
    vlb_moves=$(awk '/^[[:alnum:]@]+: [0-9]+$/ { print }' <<<"$vlb_out" | sort)
    if ! diff -u <(printf '%s\n' "$normal_moves") <(printf '%s\n' "$vlb_moves"); then
      echo "VLB parity mismatch for ${variant}" >&2
      return 1
    fi
  done
}

test_vlb_gale_smoke
test_vlb_lame_riders
test_vlb_symbol_check
test_vlb_symbol_fen
test_vlb_symbol_options
test_vlb_toot_otto_drops
test_vlb_overlap_parity

echo "VLB regressions passed"
