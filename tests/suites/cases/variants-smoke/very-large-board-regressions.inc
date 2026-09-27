#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ROOT_DIR=$(cd "${SCRIPT_DIR}/../../../.." && pwd)
source "${SCRIPT_DIR}/../../../../tests/lib/uci.sh"

ENGINE="${1:-${ROOT_DIR}/src/stockfish-vlb}"
VARIANTS="${2:-${ROOT_DIR}/src/variants.ini}"
setup_test_context "${ENGINE}" "${VARIANTS}" "vlb regressions"

# This case needs very-large-board coverage. When invoked with the suite's
# large-board engine, resolve a VLB sibling instead of silently testing
# nothing: VLB_ENGINE wins, then stockfish-vlb next to the given engine.
if [[ "$(basename "${ENGINE}")" != *vlb* && "${FSX_ENGINE_FAMILY:-}" != very-large ]]; then
  engine_dir=$(cd "$(dirname "${ENGINE}")" 2>/dev/null && pwd || dirname "${ENGINE}")
  VLB_CANDIDATE="${VLB_ENGINE:-${engine_dir}/stockfish-vlb}"
  if [[ -x "${VLB_CANDIDATE}" ]]; then
    echo "vlb regressions: using VLB engine ${VLB_CANDIDATE}"
    ENGINE="${VLB_CANDIDATE}"
  elif [[ "${FSX_ALLOW_SMALL_BOARD:-0}" == 1 ]]; then
    echo "SKIP-BOARDSIZE: VLB engine not available at ${VLB_CANDIDATE}; skip allowed by FSX_ALLOW_SMALL_BOARD=1" >&2
    exit 0
  else
    echo "vlb regressions require a very-large-board engine; got ${ENGINE} and no VLB engine at ${VLB_CANDIDATE}" >&2
    echo "build with: tests/build.sh ARCH=x86-64-modern largeboards=yes verylargeboards=yes all=yes EXE=stockfish-vlb" >&2
    echo "or set FSX_ALLOW_SMALL_BOARD=1 to allow the skip" >&2
    exit 1
  fi
fi

if [[ ! -x "${ENGINE}" ]]; then
  if [[ "${FSX_ALLOW_SMALL_BOARD:-0}" == 1 ]]; then
    echo "SKIP-BOARDSIZE: VLB engine ${ENGINE} not available; skip allowed by FSX_ALLOW_SMALL_BOARD=1" >&2
    exit 0
  fi
  echo "vlb regressions require a very-large-board engine; ${ENGINE} is not executable" >&2
  exit 1
fi

# Guard against a misnamed or stale binary: a VLB engine must load a
# very-large-only variant. Without this, every case below would skip and
# the suite would pass without testing anything.
if ! probe_variant_available "${ENGINE}" "hex-16x16" "${VARIANTS}"; then
  if [[ "${FSX_ALLOW_SMALL_BOARD:-0}" == 1 ]]; then
    echo "SKIP-BOARDSIZE: ${ENGINE} lacks very-large-board coverage; skip allowed by FSX_ALLOW_SMALL_BOARD=1" >&2
    exit 0
  fi
  echo "engine ${ENGINE} cannot load very-large-board variants (hex-16x16 missing); refusing to skip VLB coverage" >&2
  exit 1
fi

test_vlb_gale_smoke() {
  local output
  output=$(FSX_SHOW_VARIANT_LOAD_SUMMARIES=1 run_uci "$ENGINE" "$VARIANTS" gale <<'EOF' 2>&1
position startpos
go perft 1
EOF
)

  if grep -q "variants skipped because of board size limits" <<<"$output"; then
    echo "gale unexpectedly skipped because of board size limits on ${ENGINE}" >&2
    return 1
  fi

  if grep -q "No such variant: gale" <<<"$output"; then
    echo "gale unavailable in this binary" >&2
    return 1
  fi

  assert_contains_literal "$output" "info string variant gale "
  assert_contains_literal "$output" "Nodes searched: 41"
}

test_vlb_chu_dai_lion_moves() {
  local output

  output=$(run_uci "$ENGINE" "$VARIANTS" chu_shogi <<'EOF' 2>&1
position fen K10k/12/12/12/12/5p6/5p6/5W'6/12/12/12/12 w - - 0 1
go perft 1
EOF
)
  if [[ "$output" == *"unknown variant 'chu_shogi'"* || "$output" == *"No such variant: chu_shogi"* ]]; then
    echo "skip: Chu and Dai lion moves require VERY_LARGE_BOARDS"
    return 0
  fi
  assert_contains_literal "$output" "f5f6f7: 1"

  output=$(run_uci "$ENGINE" "$VARIANTS" chu_shogi <<'EOF' 2>&1
position fen K10k/12/12/12/12/12/12/5W'6/12/12/12/12 w - - 0 1
go perft 1
EOF
)
  assert_contains_literal "$output" "f5f6f5: 1"

  output=$(run_uci "$ENGINE" "$VARIANTS" chu_shogi <<'EOF' 2>&1
position fen K10k/12/12/12/12/7p4/6p5/5X'6/12/12/12/12 w - - 0 1
go perft 1
EOF
)
  assert_contains_literal "$output" "f5g6h7: 1"

  output=$(run_uci "$ENGINE" "$VARIANTS" dai_shogi <<'EOF' 2>&1
position fen K13k/15/15/15/15/15/15/15/5p9/5p9/5D"9/15/15/15/15 w - - 0 1
go perft 1
EOF
)
  assert_contains_literal "$output" "f5f6f7: 1"

  output=$(run_uci "$ENGINE" "$VARIANTS" dai_shogi <<'EOF' 2>&1
position fen K13k/15/15/15/15/15/15/15/7p8/6p9/5E"9/15/15/15/15 w - - 0 1
go perft 1
EOF
)
  assert_contains_literal "$output" "f5g6h7: 1"
}

test_vlb_chu_dai_promotion_wiring() {
  local output

  # Production promotion mapping end to end: the mover promotes, and the
  # resulting Falcon/Eagle immediately has its production two-step moves.
  # (Quiet N>N is canonically suppressed where an ordinary slide reaches the
  # same endpoint, so igui and double captures pin the wiring instead.)

  # Chu Dragon Horse promotes to Horned Falcon (igui N>S).
  output=$(run_uci "$ENGINE" "$VARIANTS" chu_shogi <<'EOF' 2>&1
position fen K11/12/12/12/5D'6/12/12/12/12/12/1p10/11k w - - 0 1 moves f8e9+ l1k1
go perft 1
EOF
)
  if [[ "$output" == *"unknown variant 'chu_shogi'"* || "$output" == *"No such variant: chu_shogi"* ]]; then
    echo "skip: Chu/Dai promotion wiring requires VERY_LARGE_BOARDS"
    return 0
  fi
  assert_contains_literal "$output" "e9e10e9: 1"

  # Chu Drunk Knight promotes to Soaring Eagle (double capture NE>NE).
  output=$(run_uci "$ENGINE" "$VARIANTS" chu_shogi <<'EOF' 2>&1
position fen K11/8p3/7p4/12/5E'6/12/12/12/12/12/1p10/11k w - - 0 1 moves f8g9+ l1k1
go perft 1
EOF
)
  assert_contains_literal "$output" "g9h10i11: 1"

  # Dai Dragon Horse promotes to Horned Falcon (igui N>S).
  output=$(run_uci "$ENGINE" "$VARIANTS" dai_shogi <<'EOF' 2>&1
position fen K14/15/15/15/15/5D'9/15/15/15/15/15/15/15/1p13/k14 w - - 0 1 moves f10e11+ a1a2
go perft 1
EOF
)
  if [[ "$output" == *"unknown variant 'dai_shogi'"* || "$output" == *"No such variant: dai_shogi"* ]]; then
    echo "skip: Dai promotion wiring requires VERY_LARGE_BOARDS"
    return 0
  fi
  assert_contains_literal "$output" "e11e12e11: 1"

  # Dai Drunk Knight promotes to Soaring Eagle (double capture NE>NE).
  output=$(run_uci "$ENGINE" "$VARIANTS" dai_shogi <<'EOF' 2>&1
position fen K14/15/8p6/7p7/15/5E'9/15/15/15/15/15/15/15/1p13/k14 w - - 0 1 moves f10g11+ a1a2
go perft 1
EOF
)
  assert_contains_literal "$output" "g11h12i13: 1"
}

test_vlb_chu_dai_dead_pieces() {
  local output

  output=$(run_uci "$ENGINE" "$VARIANTS" chu_shogi <<'EOF' 2>&1
position fen 11k/P11/12/12/12/12/12/12/12/12/12/K11 w - - 0 1
go perft 1
EOF
)
  if [[ "$output" == *"unknown variant 'chu_shogi'"* || "$output" == *"No such variant: chu_shogi"* ]]; then
    echo "skip: Chu and Dai dead-piece rules require VERY_LARGE_BOARDS"
    return 0
  fi
  assert_contains_literal "$output" "a11a12: 1"

  output=$(run_uci "$ENGINE" "$VARIANTS" dai_shogi <<'EOF' 2>&1
position fen 14k/P14/15/15/15/15/15/15/15/15/15/15/15/15/K14 w - - 0 1
go perft 1
EOF
)
  assert_contains_literal "$output" "a14a15: 1"

  # Declining on zone entry and moving onward does not grant Dai Chu's
  # special last-rank pawn retry; the pawn may become dead there.
  output=$(run_uci "$ENGINE" "$VARIANTS" dai_shogi <<'EOF' 2>&1
position fen 14k/15/15/15/15/P14/15/15/15/15/15/15/15/15/K14 w - - 0 1 moves a10a11 o15n15 a11a12 n15o15 a12a13 o15n15 a13a14 n15o15
go perft 1
EOF
)
  assert_contains_literal "$output" "a14a15: 1"
  assert_not_contains_literal "$output" "a14a15+"

  # A bare King loses against one qualifying extra piece.
  output=$(run_uci "$ENGINE" "$VARIANTS" chu_shogi <<'EOF' 2>&1
position fen 11k/12/12/12/12/12/12/12/12/12/12/KR10 b - - 0 1
go depth 1
EOF
)
  assert_contains_literal "$output" "bestmove (none)"

  # An unpromoted pawn does not count as winning material.
  output=$(run_uci "$ENGINE" "$VARIANTS" chu_shogi <<'EOF' 2>&1
position fen 11k/12/12/12/12/12/12/12/12/12/1P10/K11 b - - 0 1
go depth 1
EOF
)
  assert_not_contains "$output" "bestmove (none)"

  output=$(run_uci "$ENGINE" "$VARIANTS" chu_shogi <<'EOF' 2>&1
position fen P10k/12/12/12/12/12/12/12/12/12/12/K11 b - - 0 1
go depth 1
EOF
)
  assert_not_contains "$output" "bestmove (none)"

  # The bare King can take the sole extra piece on its next move, so the
  # immediate material-depletion result is a draw.
  output=$(run_uci "$ENGINE" "$VARIANTS" chu_shogi <<'EOF' 2>&1
position fen 10Rk/12/12/12/12/12/12/12/12/12/12/K11 b - - 0 1
go depth 1
EOF
)
  assert_contains_literal "$output" "score cp 0"
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

  out=$(FSX_SHOW_VARIANT_LOAD_SUMMARIES=1 run_uci "$ENGINE" "$variant_file" vlb-lame-clear <<'UCI' 2>&1
position startpos
go perft 1
UCI
)
  if grep -q "variants skipped because of board size limits" <<<"$out"; then
    echo "VLB lame rider variants unexpectedly skipped because of board size limits on ${ENGINE}" >&2
    return 1
  fi
  if grep -q "No such variant" <<<"$out"; then
    echo "VLB lame rider variants unavailable in this binary" >&2
    return 1
  fi

  assert_contains "$out" "info string variant vlb-lame-clear files 16 ranks 16"
  assert_contains "$out" "^m16m14: 1$"
  assert_contains "$out" "^m16k16: 1$"
  assert_contains "$out" "^m16o16: 1$"
  assert_nodes "$out" 6

  out=$(FSX_SHOW_VARIANT_LOAD_SUMMARIES=1 run_uci "$ENGINE" "$variant_file" vlb-lame-blocked <<'UCI' 2>&1
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
  engine_dir=$(cd "$(dirname "${ENGINE}")" 2>/dev/null && pwd || dirname "${ENGINE}")
  local normal_engine="${NORMAL_ENGINE:-${engine_dir}/stockfish}"
  if [[ ! -x "${normal_engine}" ]]; then
    if [[ "${FSX_ALLOW_SMALL_BOARD:-0}" == 1 ]]; then
      echo "SKIP-BOARDSIZE: normal engine unavailable for VLB overlap parity; skip allowed by FSX_ALLOW_SMALL_BOARD=1" >&2
      return 0
    fi
    echo "VLB overlap parity requires a normal-board engine; none at ${normal_engine}" >&2
    echo "build with: tests/build.sh ARCH=x86-64-modern EXE=stockfish" >&2
    return 1
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
test_vlb_chu_dai_lion_moves
test_vlb_chu_dai_promotion_wiring
test_vlb_chu_dai_dead_pieces
test_vlb_lame_riders
test_vlb_symbol_check
test_vlb_symbol_fen
test_vlb_symbol_options
test_vlb_toot_otto_drops
test_vlb_overlap_parity

echo "VLB regressions passed"
