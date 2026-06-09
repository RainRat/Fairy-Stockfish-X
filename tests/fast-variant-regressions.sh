#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "fast variant regressions"

echo "fast variant regressions started"

test_battery_chess() {
  echo "== battery-chess =="
  load_inline_variants <<'INI'
[battery-chess:chess]
captureType = hand
pieceDrops = false
promotionRequireInHand = true
promotionConsumeInHand = true
INI

  local tmp_ini="${FSX_TMP_INI}" out

  if ! variant_available "$ENGINE" battery-chess "$tmp_ini"; then
    return 0
  fi

  out=$(run_uci "$ENGINE" "$tmp_ini" battery-chess <<'EOF'
position fen 4k3/P7/8/8/8/8/8/4K3 w - - 0 1
go perft 1
EOF
)
  assert_not_contains "$out" "^a7a8"

  out=$(run_uci "$ENGINE" "$tmp_ini" battery-chess <<'EOF'
position fen 4k3/P7/8/8/8/8/8/4K3[Q] w - - 0 1
go perft 1
EOF
)
  assert_contains "$out" "^a7a8q: 1$"
  assert_not_contains "$out" "^a7a8n:"
  assert_not_contains "$out" "^a7a8r:"
  assert_not_contains "$out" "^a7a8b:"

  out=$(run_uci "$ENGINE" "$tmp_ini" battery-chess <<'EOF'
position fen 4k3/P7/8/8/8/8/8/4K3[Q] w - - 0 1 moves a7a8q
d
EOF
)
  assert_contains "$out" "Fen: Q~3k3/8/8/8/8/8/8/4K3\\[\\] b - - 0 1"
}

test_battleotk() {
  echo "== battleotk =="
  if ! variant_available "$ENGINE" battleotk "$VARIANTS"; then
    return 0
  fi

  local out
  out=$(run_uci "$ENGINE" "$VARIANTS" battleotk <<'EOF'
position startpos
go perft 1
EOF
)
  assert_contains "$out" "^e2e4n: 1$"
  assert_not_contains "$out" "^e2e4: 1$"

  out=$(run_uci "$ENGINE" "$VARIANTS" battleotk <<'EOF'
position startpos moves e2e4n
d
EOF
)
  assert_contains_literal "$out" "Fen: 8/pppppppp/8/8/4P3/8/PPPPNPPP/8 b - - 0 1"

  out=$(run_uci "$ENGINE" "$VARIANTS" battleotk <<'EOF'
position fen 8/6P1/8/8/8/8/8/8 w - - 0 1 moves g7g8n
d
EOF
)
  assert_contains_literal "$out" "Fen: 6N1/6N1/8/8/8/8/8/8 b - - 0 1"

  out=$(run_uci "$ENGINE" "$VARIANTS" battleotk <<'EOF'
position fen 8/ppnppppp/8/2n5/2pP4/4PP2/PPPNNNPP/8 b - d3 0 3
go perft 1
EOF
)
  assert_contains "$out" "^c4d3: 1$"

  out=$(run_uci "$ENGINE" "$VARIANTS" battleotk <<'EOF'
position fen 8/ppnppppp/8/2n5/2pP4/4PP2/PPPNNNPP/8 b - d3 0 3 moves c4d3
d
EOF
)
  assert_contains_literal "$out" "Fen: 8/ppnppppp/8/2n5/8/3pPP2/PPPNNNPP/8 w - - 0 4"

  out=$(run_uci "$ENGINE" "$VARIANTS" battleotk <<'EOF'
position startpos
go depth 1
EOF
)
  assert_contains "$out" "^bestmove "
  assert_not_contains_literal "$out" "bestmove (none)"

  out=$(run_uci "$ENGINE" "$VARIANTS" battleotk <<'EOF'
position fen 8/8/8/8/8/8/1k6/K7 w - - 0 1 moves a1b2
d
EOF
)
  assert_contains_literal "$out" "Fen: 8/8/8/8/8/8/1K6/8 b - - 0 1"

  out=$(run_uci "$ENGINE" "$VARIANTS" battleotk <<'EOF'
position fen 8/8/8/8/8/8/1k6/K7 w - - 0 1
go depth 1
EOF
)
  assert_contains "$out" "^bestmove "
  assert_not_contains_literal "$out" "bestmove (none)"

  out=$(run_uci "$ENGINE" "$VARIANTS" battleotk <<'EOF'
position fen 8/8/8/8/8/8/1k6/K7 w - - 0 1
go depth 2
EOF
)
  assert_contains "$out" "^bestmove "
  assert_not_contains_literal "$out" "bestmove (none)"

  out=$(run_uci "$ENGINE" "$VARIANTS" battleotk <<'EOF'
position fen 8/8/8/8/8/8/1k6/K7 w - - 0 1
go depth 2 searchmoves a1b1 a1a2 a1b2
EOF
)
  assert_contains_literal "$out" "bestmove a1b2"

  out=$(run_uci "$ENGINE" "$VARIANTS" battleotk <<'EOF'
position fen 8/8/8/8/8/8/1kk5/K7 w - - 0 1 moves a1b2
d
EOF
)
  assert_contains_literal "$out" "Fen: 8/8/8/8/8/8/1kk5/K7 w - - 0 1"

  out=$(run_uci "$ENGINE" "$VARIANTS" battleotk <<'EOF'
position fen 8/8/8/8/8/8/1kk5/K7 w - - 0 1
go depth 1
EOF
)
  assert_contains_literal "$out" "bestmove (none)"

  out=$(run_uci "$ENGINE" "$VARIANTS" battleotk <<'EOF'
position fen K7/R6q/7r/8/8/8/6Q1/8 b - - 0 1 moves h7h8k
d
go perft 1
EOF
)
  assert_contains_literal "$out" "Fen: K7/R6q/7r/8/8/8/6Q1/8 b - - 0 1"
  assert_not_contains "$out" "^h7h8k:"
  assert_contains "$out" "^h7g7k: 1$"
}

test_beast_chess() {
  echo "== beast-chess =="
  if ! variant_available "$ENGINE" beast-chess "$VARIANTS"; then
    return 0
  fi

  local tmp_ini out
  load_inline_variants <<'INI'
[beast-pieces:chess]
customPiece1 = e:O
customPiece2 = g:NL
customPiece3 = h:M
pieceToCharTable = P..Q....EGH.Kp..q....egh.k
castling = false
doubleStep = false
promotionPieceTypes = qegh
INI
  tmp_ini="${FSX_TMP_INI}"

  out=$(run_uci "$ENGINE" "$VARIANTS" beast-chess <<'EOF'
position startpos
d
EOF
)
  assert_contains "$out" "Fen: eghqkhge/pppppppp/8/8/8/8/PPPPPPPP/EGHQKHGE w KQkq - 0 1"

  out=$(run_uci "$ENGINE" "$tmp_ini" beast-pieces <<'EOF'
position fen 4k3/8/8/8/3E4/8/8/4K3 w - - 0 1
go perft 1
EOF
)
  assert_contains "$out" "^d4h5: 1$"
  assert_not_contains "$out" "^d4d5:"

  out=$(run_uci "$ENGINE" "$tmp_ini" beast-pieces <<'EOF'
position fen 4k3/8/8/8/3H4/8/8/4K3 w - - 0 1
go perft 1
EOF
)
  assert_contains "$out" "^d4g8: 1$"
  assert_not_contains "$out" "^d4h5:"

  out=$(run_uci "$ENGINE" "$tmp_ini" beast-pieces <<'EOF'
position fen 4k3/8/8/8/3G4/8/8/4K3 w - - 0 1
go perft 1
EOF
)
  assert_contains "$out" "^d4b5: 1$"
  assert_contains "$out" "^d4a5: 1$"
}

test_seega() {
  echo "== seega =="
  if ! variant_available "$ENGINE" seega "$VARIANTS"; then
    return 0
  fi

  local out
  out=$(run_uci "$ENGINE" "$VARIANTS" seega <<'EOF'
position startpos moves D@a1
go perft 1
EOF
)
  assert_contains "$out" "^0000: 1$"
  assert_not_contains "$out" "^D@"

  out=$(run_uci "$ENGINE" "$VARIANTS" seega <<'EOF'
position startpos moves D@a1 0000
go perft 1
EOF
)
  assert_contains "$out" "^D@"
  assert_not_contains "$out" "^0000: 1$"

  out=$(run_uci "$ENGINE" "$VARIANTS" seega <<'EOF'
position startpos moves D@a1 0000 D@b1
d
EOF
)
  assert_contains "$out" "^Fen: .* b "

  out=$(run_uci "$ENGINE" "$VARIANTS" seega <<'EOF'
position fen d4/5/1D1dD/5/d4 w - - 0 1 moves b3c3
d
EOF
)
  assert_contains_literal "$out" "Fen: d4/5/2D1D/5/d4[] b - - 1 1"

  out=$(run_uci "$ENGINE" "$VARIANTS" seega <<'EOF'
position fen 5/2D2/1DdD1/D1D2/dD3 b - - 0 1
go perft 1
EOF
)
  assert_contains "$out" "^0000: 1$"

  out=$(run_uci "$ENGINE" "$VARIANTS" seega <<'EOF'
position fen 5/5/5/5/1D3[] b - - 0 1
go movetime 20
EOF
)
  assert_contains "$out" "^info depth 0 score mate 0$"
  assert_contains_literal "$out" "bestmove (none)"
}

test_qubic() {
  echo "== qubic =="
  if ! variant_available "$ENGINE" qubic "$VARIANTS"; then
    return 0
  fi

  local out
  out=$(run_uci "$ENGINE" "$VARIANTS" qubic <<<'position fen 8/8/8/8/8/8/8/8[pppppppppppppppppppppppppppppppp] b - - 0 1
go perft 1')
  assert_nodes "$out" 64

  out=$(run_uci "$ENGINE" "$VARIANTS" qubic <<<'position fen 8/8/8/P3P3/8/8/8/P3P3[pppppppppppppppppppppppppppp] b - - 0 1
go perft 1')
  assert_nodes "$out" 0

  out=$(run_uci "$ENGINE" "$VARIANTS" qubic <<<'position fen 8/8/8/8/8/8/8/P7[ppppppppppppppppppppppppppppppp] b - - 0 1
go perft 1')
  assert_nodes "$out" 63

  out=$(run_uci "$ENGINE" "$VARIANTS" qubic <<<'position fen 7P/2P5/8/8/8/8/5P2/P7[pppppppppppppppppppppppppppp] b - - 0 1
go perft 1')
  assert_nodes "$out" 0
}

test_haynie_leapers() {
  echo "== haynie-leapers =="
  if ! variant_available "$ENGINE" haynie-leapers "$VARIANTS"; then
    return 0
  fi

  local out
  out=$(run_uci "$ENGINE" "$VARIANTS" haynie-leapers <<'EOF'
position startpos
go perft 1
EOF
)
  assert_contains "$out" "^Nodes searched: 28$"
  assert_contains "$out" "^a1c4: 1$"
  assert_contains "$out" "^c1b3: 1$"
  assert_contains "$out" "^b1a4: 1$"

  out=$(run_uci "$ENGINE" "$VARIANTS" haynie-leapers <<'EOF'
position fen k7/7P/8/8/8/8/8/7K w - - 0 1
go perft 1
EOF
)
  assert_contains "$out" "^h7h8z: 1$"
  assert_contains "$out" "^h7h8c: 1$"
  assert_contains "$out" "^h7h8w: 1$"
  assert_not_contains "$out" "^h7h8: 1$"
}

test_kopano() {
  echo "== kopano =="
  if ! variant_available "$ENGINE" kopano "$VARIANTS"; then
    return 0
  fi

  local out
  out=$(run_uci "$ENGINE" "$VARIANTS" kopano <<'EOF'
position startpos
go perft 1
EOF
)
  assert_nodes "$out" 64

  out=$(run_uci "$ENGINE" "$VARIANTS" kopano <<'EOF'
position startpos moves P@b1
go perft 1
EOF
)
  assert_contains "$out" "^P@a2: 1$"
  assert_not_contains "$out" "^P@b1: 1$"

  out=$(run_uci "$ENGINE" "$VARIANTS" kopano <<'EOF'
position fen 8/8/8/8/8/8/1P6/8[Pp] w - - 0 1
go perft 1
EOF
)
  assert_not_contains "$out" "^P@c3: 1$"

  out=$(run_uci "$ENGINE" "$VARIANTS" kopano <<'EOF'
position fen 8/8/8/8/3p4/8/1P6/8[Pp] w - - 0 1
go perft 1
EOF
)
  assert_contains "$out" "^P@c3: 1$"

  out=$(run_uci "$ENGINE" "$VARIANTS" kopano <<'EOF'
position fen 8/8/8/8/2pP4/3p4/8/8[Pp] w - - 0 1
go perft 1
EOF
)
  assert_not_contains "$out" "^P@c3: 1$"

  out=$(run_uci "$ENGINE" "$VARIANTS" kopano <<'EOF'
position fen 7p/6p1/5p2/4p3/3p4/2p5/1p6/p7 w - - 0 1
go perft 1
EOF
)
  assert_nodes "$out" 0
}

test_konobi() {
  echo "== konobi =="
  if ! variant_available "$ENGINE" konobi "$VARIANTS"; then
    return 0
  fi

  local out
  out=$(run_uci "$ENGINE" "$VARIANTS" konobi <<'EOF'
position startpos
go perft 1
EOF
)
  assert_contains "$out" "^Nodes searched: 64$"

  out=$(run_uci "$ENGINE" "$VARIANTS" konobi <<'EOF'
position startpos moves P@b1
go perft 1
EOF
)
  assert_contains "$out" "^P@a2: 1$"
  assert_not_contains "$out" "^P@b1: 1$"

  out=$(run_uci "$ENGINE" "$VARIANTS" konobi <<'EOF'
position fen 8/8/8/8/3p4/8/1P6/8[Pp] w - - 0 1
go perft 1
EOF
)
  assert_not_contains "$out" "^P@c3: 1$"

  out=$(run_uci "$ENGINE" "$VARIANTS" konobi <<'EOF'
position fen 8/8/8/8/3p4/1p6/ppp5/pp6[Pp] w - - 0 1
go perft 1
EOF
)
  assert_contains "$out" "^P@c3: 1$"

  out=$(run_uci "$ENGINE" "$VARIANTS" konobi <<'EOF'
position fen 8/8/8/8/2pP4/3p4/8/8[Pp] w - - 0 1
go perft 1
EOF
)
  assert_not_contains "$out" "^P@c3: 1$"
}

test_connect_region3() {
  echo "== connect-region3 =="
  load_inline_variants <<'INI'
[mini-y:fairy]
maxRank = 5
maxFile = 5
hexBoard = true
pieceToCharTable = -
king = -
customPiece1 = s:m
pieceDrops = true
mustDrop = true
openingSwapDrop = true
connectPieceTypes = s
connectHorizontal = true
connectVertical = true
connectDiagonal = true
connectNorthEast = false
connectSouthEast = true
connectRegion1White = a1 b1 c1 d1 e1
connectRegion2White = a1 b2 c3 d4 e5
connectRegion3White = e1 e2 e3 e4 e5
connectRegion1Black = a1 b1 c1 d1 e1
connectRegion2Black = a1 b2 c3 d4 e5
connectRegion3Black = e1 e2 e3 e4 e5
nMoveRule = 0
startFen = ****1/***2/**3/*4/5[SSSSSSSSSSSSSSSsssssssssssssss] b - - 0 1
INI

  local tmp_ini="${FSX_TMP_INI}" out
  if ! variant_available "$ENGINE" mini-y "$tmp_ini"; then
    return 0
  fi

  out=$(run_uci "$ENGINE" "$tmp_ini" mini-y <<'EOF'
position startpos
go perft 1
EOF
)
  assert_nodes "$out" 15

  out=$(run_uci "$ENGINE" "$tmp_ini" mini-y <<'EOF'
position fen ^^^^b/^^^1b/^^2b/^3b/bbbbb w - - 0 1
go perft 1
EOF
)
  assert_nodes "$out" 0

  out=$(run_uci "$ENGINE" "$tmp_ini" mini-y <<'EOF'
position fen ^^^^1/^^^2/^^3/^b1b1/b1b1b[S] w - - 0 1
go perft 1
EOF
)
  assert_contains "$out" "^S@b1: 1$"
}

test_connect_adjudication_edges() {
  echo "== connect adjudication edges =="
  load_inline_variants <<'INI'
[toroidal-connect-seam:fairy]
maxRank = 5
maxFile = e
toroidal = true
pieceToCharTable = -
king = -
customPiece1 = s:m
connectN = 4
connectPieceTypes = s
connectHorizontal = true
connectVertical = false
connectDiagonal = false
nMoveRule = 0
startFen = 5/5/5/5/SS1SS b - - 0 1

[type-goal-palindrome:fairy]
maxRank = 1
maxFile = d
pieceToCharTable = -
king = -
customPiece1 = t:m
customPiece2 = o:m
connectN = 0
connectGoalByType = true
connectPieceGoalWhite = t o o t
connectPieceGoalBlack = t t t t
connectHorizontal = true
connectVertical = false
connectDiagonal = false
nMoveRule = 0
startFen = TOOT b - - 0 1

[type-goal-reverse:fairy]
maxRank = 1
maxFile = c
pieceToCharTable = -
king = -
customPiece1 = a:m
customPiece2 = b:m
customPiece3 = c:m
connectN = 0
connectGoalByType = true
connectPieceGoalWhite = a b c
connectPieceGoalBlack = a a a
connectHorizontal = true
connectVertical = false
connectDiagonal = false
nMoveRule = 0
startFen = CBA b - - 0 1

[connect-region-seed-overlap:fairy]
maxRank = 3
maxFile = c
pieceToCharTable = -
king = -
customPiece1 = s:m
connectPieceTypes = s
connectHorizontal = true
connectVertical = true
connectDiagonal = false
connectRegion1White = a1
connectRegion2White = a1
connectRegion3White = a1
nMoveRule = 0
startFen = 3/3/S2 b - - 0 1

[connect-group-three:fairy]
maxRank = 3
maxFile = c
pieceToCharTable = -
king = -
customPiece1 = s:m
connectPieceTypes = s
connectGroup = 3
connectHorizontal = true
connectVertical = true
connectDiagonal = false
nMoveRule = 0
startFen = 3/3/SSS b - - 0 1
INI

  local tmp_ini="${FSX_TMP_INI}" out

  for variant in \
      toroidal-connect-seam \
      type-goal-palindrome \
      type-goal-reverse \
      connect-region-seed-overlap \
      connect-group-three; do
    if ! variant_available "$ENGINE" "$variant" "$tmp_ini"; then
      return 0
    fi

    out=$(run_uci "$ENGINE" "$tmp_ini" "$variant" <<'EOF'
position startpos
go perft 1
EOF
)
    assert_contains "$out" "^Nodes searched: 0$"
  done
}

test_flank_chess() {
  echo "== flank-chess =="
  if ! variant_available "$ENGINE" flank-chess "$VARIANTS"; then
    return 0
  fi

  local out
  out=$(run_uci "$ENGINE" "$VARIANTS" flank-chess <<'EOF'
setoption name UCI_Variant value flank-chess
position startpos
d
EOF
)
  assert_contains "$out" "Fen: 10/1rzsqkszr1/1ppppappp1/10/10/10/1PPPPAPPP1/1RZSQKSZR1/10 w KQkq - 0 1"

  out=$(run_uci "$ENGINE" "$VARIANTS" flank-chess <<'EOF'
setoption name UCI_Variant value flank-chess
position fen 10/10/10/10/10/10/5r4/5A4/10 b - - 0 1
go perft 1
EOF
)
  assert_not_contains "$out" "^f3f2: 1$"

  out=$(run_uci "$ENGINE" "$VARIANTS" flank-chess <<'EOF'
setoption name UCI_Variant value flank-chess
position fen 10/10/10/10/10/10/5p4/5A4/10 b - - 0 1
go perft 1
EOF
)
  assert_contains "$out" "^f3f2: 1$"

  out=$(run_uci "$ENGINE" "$VARIANTS" flank-chess <<'EOF'
setoption name UCI_Variant value flank-chess
position fen 10/10/10/10/10/10/5a4/5A4/10 b - - 0 1
go perft 1
EOF
)
  assert_contains "$out" "^f3f2: 1$"

  out=$(run_uci "$ENGINE" "$VARIANTS" flank-chess <<'EOF'
setoption name UCI_Variant value flank-chess
position fen 10/10/10/10/10/10/10/1R3K2R1/10 w KQ - 0 1
go perft 1
EOF
)
  assert_contains "$out" "^f2h2: 1$"
  assert_contains "$out" "^f2d2: 1$"
}

test_crazy_cavalier() {
  echo "== crazy-cavalier =="
  if ! variant_available "$ENGINE" crazy-cavalier "$VARIANTS"; then
    return 0
  fi

  local out
  out=$(run_uci "$ENGINE" "$VARIANTS" crazy-cavalier <<'EOF'
position startpos
go perft 1
EOF
)
  assert_contains "$out" "info string variant crazy-cavalier files 9 ranks 10"

  out=$(run_uci "$ENGINE" "$VARIANTS" crazy-cavalier <<'EOF'
position fen 9/9/9/9/9/9/9/4D4/9/5k3 w - - 0 1
go perft 1
EOF
)
  assert_contains "$out" "^e3d3: 1$"
  assert_contains "$out" "^e3f3: 1$"

  out=$(run_uci "$ENGINE" "$VARIANTS" crazy-cavalier <<'EOF'
position fen 5k3/9/4D4/9/9/9/9/9/9/5K3 w - - 0 1
go perft 1
EOF
)
  assert_contains "$out" "^e8d8c: 1$"
  assert_not_contains "$out" "^e8d8q: 1$"

  out=$(run_uci "$ENGINE" "$VARIANTS" crazy-cavalier <<'EOF'
position fen 5k3/9/9/9/4d4/5D3/9/9/9/5K3 w - - 0 1 moves f5e6
d
EOF
)
  assert_contains "$out" "Fen: 5k3/9/9/9/4D4/9/9/9/9/5K3\\[D\\] b - - 0 1"
}

test_constabulary_chess() {
  echo "== constabulary-chess =="
  if ! variant_available "$ENGINE" constabulary-chess "$VARIANTS"; then
    return 0
  fi

  local out
  out=$(run_uci "$ENGINE" "$VARIANTS" constabulary-chess <<'EOF'
position startpos
d
EOF
)
  assert_contains "$out" "Fen: wxeiiexw/rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR/WXEIIEXW w KQkq - 0 1"

  out=$(run_uci "$ENGINE" "$VARIANTS" constabulary-chess <<'EOF'
position fen 8/8/8/8/8/8/8/8/R3K2R/8 w KQ - 0 1 moves e2g2
d
EOF
)
  assert_contains "$out" "Fen: 8/8/8/8/8/8/8/8/R4RK1/8 b - - 1 1"

  out=$(run_uci "$ENGINE" "$VARIANTS" constabulary-chess <<'EOF'
position fen 7k/P7/8/8/8/8/8/8/8/7K w - - 0 1
go perft 1
EOF
)
  assert_contains "$out" "^a9a10q: 1$"
  assert_contains "$out" "^a9a10w: 1$"
  assert_contains "$out" "^a9a10x: 1$"
  assert_contains "$out" "^a9a10e: 1$"
  assert_contains "$out" "^a9a10i: 1$"
}

test_little_trio() {
  echo "== little-trio =="
  if ! variant_available "$ENGINE" little-trio "$VARIANTS"; then
    return 0
  fi

  local out
  out=$(run_uci "$ENGINE" "$VARIANTS" little-trio <<'EOF'
position startpos
go perft 1
EOF
)
  assert_contains "$out" "f1f6: 1"

  out=$(run_uci "$ENGINE" "$VARIANTS" little-trio <<'EOF'
position fen s3k2/7/7/7/7/7/R3K2[] w - - 0 1 moves a1a7
d
EOF
)
  assert_contains "$out" "Fen: R3k2/7/7/7/7/7/4K2\\[S\\] b - - 0 1"

  out=$(run_uci "$ENGINE" "$VARIANTS" little-trio <<'EOF'
position fen c3k2/7/7/7/7/7/R3K2[] w - - 0 1 moves a1a7
d
EOF
)
  assert_contains "$out" "Fen: R3k2/7/7/7/7/7/4K2\\[\\] b - - 0 1"

  out=$(run_uci "$ENGINE" "$VARIANTS" little-trio <<'EOF'
position fen 4k2/7/7/7/7/7/4K2[L] w - - 0 1
go perft 1
EOF
)
  assert_contains "$out" "L@a6: 1"
  assert_not_contains "$out" "L@a7: 1"
}

test_nd_tictactoe() {
  echo "== nd-tictactoe =="
  if fsx_variant_exceeds_build_limits "$ENGINE" tictactoe-3d "$VARIANTS"; then
    return 0
  fi
  if ! variant_available "$ENGINE" tictactoe-3d "$VARIANTS"; then
    return 0
  fi
  if fsx_variant_exceeds_build_limits "$ENGINE" tictactoe-4d "$VARIANTS"; then
    return 0
  fi
  if ! variant_available "$ENGINE" tictactoe-4d "$VARIANTS"; then
    return 0
  fi

  local out
  out=$(run_uci "$ENGINE" "$VARIANTS" tictactoe-3d <<'EOF'
position fen 3/3/3/3/3/3/3/3/P1P[PPPPPPPPPPppppppppppppp] w - - 0 1 moves P@b1
go perft 1
EOF
)
  assert_contains "$out" "^Nodes searched: 0$"

  out=$(run_uci "$ENGINE" "$VARIANTS" tictactoe-3d <<'EOF'
position fen 3/3/3/3/1P1/3/3/3/P2[PPPPPPPPPPppppppppppppp] w - - 0 1 moves P@c9
go perft 1
EOF
)
  assert_contains "$out" "^Nodes searched: 0$"

  out=$(run_uci "$ENGINE" "$VARIANTS" tictactoe-4d <<'EOF'
position fen 9/9/9/9/9/9/9/9/P1P6[PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPpppppppppppppppppppppppppppppppppppppppp] w - - 0 1 moves P@b1
go perft 1
EOF
)
  assert_contains "$out" "^Nodes searched: 0$"

  out=$(run_uci "$ENGINE" "$VARIANTS" tictactoe-4d <<'EOF'
position fen 9/9/9/9/1P7/9/9/9/P8[PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPpppppppppppppppppppppppppppppppppppppppp] w - - 0 1 moves P@c9
go perft 1
EOF
)
  assert_contains "$out" "^Nodes searched: 0$"
}

test_battery_chess
test_battleotk
test_beast_chess
test_seega
test_qubic
test_haynie_leapers
test_kopano
test_konobi
test_connect_region3
test_connect_adjudication_edges
test_flank_chess
test_crazy_cavalier
test_constabulary_chess
test_little_trio
test_nd_tictactoe

echo "fast variant regressions passed"
