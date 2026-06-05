#!/bin/bash

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
export ROOT_DIR
source "${ROOT_DIR}/tests/lib/uci.sh"
setup_test_context "${1:-}" "${2:-}" "geometry regressions"

test_cylinder() {
  if ! variant_available "$ENGINE" cylinder "$VARIANTS"; then
    echo "cylinder variant not available in this build; skipping cylinder regression"
    return 0
  fi

  local out
  out=$(run_perft cylinder "4k3/8/8/8/8/8/8/R3K3 w - - 0 1" 1)
  assert_contains "$out" "^a1h1: 1$"

  out=$(run_perft cylinder "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1" 1)
  assert_not_contains "$out" "^e1g1: 1$"
  assert_not_contains "$out" "^e1c1: 1$"

  if variant_available "$ENGINE" cylinder-castling "$VARIANTS"; then
    out=$(run_perft cylinder-castling "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1" 1)
    assert_contains "$out" "^e1g1: 1$"
    assert_contains "$out" "^e1c1: 1$"
  else
    echo "cylinder-castling variant not available in this build; skipping castling-specific cylinder checks"
  fi
}

test_toroidal_chess() {
  if ! variant_available "$ENGINE" toroidal-chess "$VARIANTS"; then
    echo "toroidal-chess variant not available in this build; skipping toroidal-chess regression"
    return 0
  fi

  local out
  out=$(run_display toroidal-chess startpos)
  assert_contains "$out" "Fen: r1b2b1r/pp4pp/n1pqkp1n/3pp3/3PP3/N1PQKP1N/PP4PP/R1B2B1R w - - 0 1"

  out=$(run_perft toroidal-chess "1k6/8/8/8/8/8/4K3/R7 w - - 0 1" 1)
  assert_contains "$out" "^a1h1: 1$"
  assert_contains "$out" "^a1a8: 1$"

  out=$(run_perft toroidal-chess "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1" 1)
  assert_not_contains "$out" "^e1g1: 1$"
  assert_not_contains "$out" "^e1c1: 1$"
}

test_nightrider_evasion() {
  if ! variant_available "$ENGINE" nightrider "$VARIANTS"; then
    echo "nightrider variant not available in this build; skipping nightrider evasion regression"
    return 0
  fi

  local out
  out=$(run_uci "$ENGINE" "$VARIANTS" nightrider <<'EOF'
position fen 8/8/8/8/8/4K3/8/n1R5 w - - 0 1
go depth 1 searchmoves c1c2
EOF
)
  assert_contains "$out" "bestmove c1c2"

  out=$(run_uci "$ENGINE" "$VARIANTS" nightrider <<'EOF'
position fen 8/8/8/8/8/4K3/8/n1R5 w - - 0 1
go depth 1 searchmoves c1a1
EOF
)
  assert_contains "$out" "bestmove c1a1"

  out=$(run_uci "$ENGINE" "$VARIANTS" nightrider <<'EOF'
position fen 8/8/8/8/8/4K3/6n1/2R5 w - - 0 1
go depth 1 searchmoves c1c2
EOF
)
  assert_contains_literal "$out" "bestmove (none)"
}

test_move_morph() {
  if ! variant_available "$ENGINE" bishop-knight-morph-factor "$VARIANTS"; then
    echo "bishop-knight-morph-factor variant not available in this build; skipping move morph regression"
    return 0
  fi

  local out
  out=$(run_uci "$ENGINE" "$VARIANTS" bishop-knight-morph-factor <<'EOF'
position startpos moves g1f3
d
EOF
)
  assert_contains "$out" "Fen: rnbqkbnr/pppppppp/8/8/8/5B2/PPPPPPPP/RNBQKB1R b KQkq - 1 1"

  out=$(run_uci "$ENGINE" "$VARIANTS" bishop-knight-morph-factor <<'EOF'
position fen 4k3/8/8/8/8/8/8/2B1K3 w - - 0 1 moves c1g5
d
EOF
)
  assert_contains "$out" "Fen: 4k3/8/8/6N1/8/8/8/4K3 b - - 1 1"
}

test_edge_insert() {
  load_inline_variants <<'INI'
[edge-insert-demo:chess]
maxRank = 5
maxFile = e
pieceToCharTable = -
king = -
customPiece1 = a:mW
startFen = 5/5/5/5/5[AAAAAAAAAA] w - - 0 1
pieceDrops = true
mustDrop = true
checking = false
pushingStrength = a:5
pushFirstColor = either
pushingRemoves = shove
edgeInsertOnly = true
edgeInsertTypes = a
edgeInsertRegionWhite = a5 b5 c5 d5 e5 a1 a2 a3 a4 a5
edgeInsertFromWhite = top left
dropRegionWhite = a5 b5 c5 d5 e5 a1 a2 a3 a4 a5
INI

  local tmp_ini="${FSX_TMP_INI}"
  local out

  out=$(run_uci "$ENGINE" "$tmp_ini" edge-insert-demo <<'EOF'
position fen A4/5/5/5/5[AAAAAAAAA] w - - 0 1
go perft 1
EOF
)
  assert_contains "$out" "^A@a4,b4: 1$"
  assert_contains "$out" "^A@b5,b4: 1$"

  out=$(run_uci "$ENGINE" "$tmp_ini" edge-insert-demo <<'EOF'
position fen A4/5/5/5/5[AAAAAAAAA] w - - 0 1 moves A@a4,b4
d
EOF
)
  assert_contains_literal "$out" "Fen: A4/A4/5/5/5[AAAAAAAA] b - - 0 1"

  out=$(run_uci "$ENGINE" "$tmp_ini" edge-insert-demo <<'EOF'
position fen A4/5/5/5/5[AAAAAAAAA] w - - 0 1 moves A@b5,b4
d
EOF
)
  assert_contains_literal "$out" "Fen: AA3/5/5/5/5[AAAAAAAA] b - - 0 1"

  out=$(run_uci "$ENGINE" "$tmp_ini" edge-insert-demo <<'EOF'
position startpos moves A@a1
d
EOF
)
  assert_contains_literal "$out" "Fen: 5/5/5/5/5[AAAAAAAAAA] w - - 0 1"
}

test_cylinder
test_toroidal_chess
test_nightrider_evasion
test_move_morph
test_edge_insert
