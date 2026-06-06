#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
export ROOT_DIR
source "${ROOT_DIR}/tests/lib/uci.sh"
setup_test_context "${1:-}" "${2:-}" "special regressions"

test_changing_color_drop_capture() {
  load_inline_variants <<'EOF'
[capture-drop-color:chess]
customPiece1 = u:W
pieceDrops = true
captureDrops = u
changingColorTrigger = capture
changingColorPieceTypes = u
EOF

  local out
  out=$(run_uci "$ENGINE" "$FSX_TMP_INI" capture-drop-color <<'EOF'
position fen 7k/8/8/8/4p3/8/8/7K[U] w - - 0 1 moves U@e4
d
EOF
)

  assert_contains_literal "$out" "Fen: 7k/8/8/8/4u3/8/8/7K[] b"
}

test_dead_pieces() {
  local out
  out=$(run_uci "$ENGINE" "$VARIANTS" fatal-giveaway <<'EOF'
position fen 4k3/8/8/4p3/4R3/8/8/4K3 w - - 0 1 moves e4e5
d
EOF
)
  assert_contains_literal "$out" "Fen: 4k3/8/8/4^3/8/8/8/4K3 b - - 0 1"

  out=$(run_uci "$ENGINE" "$VARIANTS" fatal-giveaway <<'EOF'
position fen 4k3/8/8/4^3/3P4/8/8/4K3 w - - 0 1
go perft 1
EOF
)
  assert_contains "$out" "^d4e5: 1$"

  out=$(run_uci "$ENGINE" "$VARIANTS" fatal-giveaway <<'EOF'
position fen 4k3/8/8/4^3/8/8/8/4K3 b - - 0 1
go perft 1
EOF
)
  assert_not_contains "$out" "^e5"
}

test_material_hand_cache() {
  local same_process fresh_process same_final fresh_final

  same_process=$(run_uci "$ENGINE" "$VARIANTS" crazyhouse <<'EOF'
position fen 4k3/8/8/8/8/8/8/4K3[] w - - 0 1
eval
position fen 4k3/8/8/8/8/8/8/4K3[QQQ] w - - 0 1
eval
EOF
)

  fresh_process=$(run_uci "$ENGINE" "$VARIANTS" crazyhouse <<'EOF'
position fen 4k3/8/8/8/8/8/8/4K3[QQQ] w - - 0 1
eval
EOF
)

  same_final=$(printf '%s\n' "$same_process" | grep "Final evaluation" | tail -n 1)
  fresh_final=$(printf '%s\n' "$fresh_process" | grep "Final evaluation" | tail -n 1)

  if [[ -z "${same_final}" || -z "${fresh_final}" || "${same_final}" != "${fresh_final}" ]]; then
    printf '%s\n' "${same_process}"
    printf '%s\n' "${fresh_process}"
    return 1
  fi
}

test_sacrifice() {
  local out
  out=$(run_uci "$ENGINE" "$VARIANTS" sacrifice <<'EOF'
position startpos
go perft 1
EOF
)
  assert_contains "$out" "^h2h2x: 1$"
  assert_contains "$out" "^a2a2x: 1$"
  assert_not_contains "$out" "^g1g1x: 1$"
}

test_stationary_capture() {
  load_inline_variants <<'EOF'
[stationary-capture-demo:chess]
customPiece1 = a:c^W
customPiece2 = b:mWc^K
EOF

  local out
  out=$(run_uci "$ENGINE" "$FSX_TMP_INI" stationary-capture-demo <<'EOF'
position fen 4k3/8/8/4p3/4A3/8/8/4K3 w - - 0 1 moves e4e5
d
EOF
)
  assert_contains_literal "$out" "Fen: 4k3/8/8/8/4A3/8/8/4K3 b - - 0 1"

  out=$(run_uci "$ENGINE" "$FSX_TMP_INI" stationary-capture-demo <<'EOF'
position fen 4k3/8/8/4p3/4B3/8/8/4K3 w - - 0 1 moves e4e5
d
EOF
)
  assert_contains_literal "$out" "Fen: 4k3/8/8/8/4B3/8/8/4K3 b - - 0 1"

  out=$(run_uci "$ENGINE" "$FSX_TMP_INI" stationary-capture-demo <<'EOF'
position fen 4k3/8/8/8/4B3/8/8/4K3 w - - 0 1 moves e4e5
d
EOF
)
  assert_contains_literal "$out" "Fen: 4k3/8/8/4B3/8/8/8/4K3 b - - 1 1"
}

test_self_destruct_special() {
  local out
  out=$(run_uci "$ENGINE" "$VARIANTS" bombardment <<'EOF'
position fen 8/8/2mmm3/2mMm3/2mmm3/8/8/M7 w - - 0 1 moves d5d5x
d
EOF
)
  assert_contains_literal "$out" "Fen: 8/8/8/8/8/8/8/M7 b - - 0 1"
}

test_toot_otto() {
  local out
  out=$(run_uci "$ENGINE" "$VARIANTS" toot-otto <<'EOF'
position startpos
go perft 1
EOF
)
  assert_contains_literal "$out" "info string variant toot-otto"
  assert_contains_literal "$out" "T@a1: 1"
  assert_contains_literal "$out" "O@a1: 1"
}

test_hardlimit() {
  load_inline_variants <<'EOF'
[hardlimit-test:chess]
nMoveHardLimitRule = 200
nMoveHardLimitRuleValue = draw
EOF

  local out
  out=$(run_uci "$ENGINE" "$FSX_TMP_INI" hardlimit-test <<'EOF'
position fen 8/8/8/8/8/8/8/K6k w - - 0 200
go depth 1
EOF
)
  assert_contains_literal "$out" "bestmove a1b1"

  out=$(run_uci "$ENGINE" "$FSX_TMP_INI" hardlimit-test <<'EOF'
position fen 8/8/8/8/8/8/8/K6k w - - 0 201
go depth 1
EOF
)
  assert_contains_literal "$out" "bestmove (none)"
}

test_hindustani() {
  local out
  out=$(run_uci "$ENGINE" "$VARIANTS" hindustani <<'EOF'
position startpos
go perft 1
EOF
)
  assert_contains "$out" "^e1d3: 1$"
  assert_contains "$out" "^e1f3: 1$"

  out=$(run_uci "$ENGINE" "$VARIANTS" hindustani <<'EOF'
position startpos moves e1d3
go perft 1
EOF
)
  assert_not_contains "$out" "^d3b2: 1$"
  assert_not_contains "$out" "^d3f2: 1$"

  out=$(run_uci "$ENGINE" "$VARIANTS" hindustani <<'EOF'
position fen 3k4/8/8/8/8/8/r7/4K3 b E - 0 1 moves a2e2
go perft 1
EOF
)
  assert_not_contains "$out" "^e1d3: 1$"
  assert_not_contains "$out" "^e1f3: 1$"
}

test_jedi_chess() {
  local out
  out=$(run_uci "$ENGINE" "$VARIANTS" jedi-chess <<'EOF'
position startpos
go perft 1
EOF
)
  assert_contains_literal "$out" "Nodes searched: 20"

  out=$(run_uci "$ENGINE" "$VARIANTS" jedi-chess <<'EOF'
position fen 4s3/4q3/8/8/8/8/8/4K3 b - - 0 1
go perft 1
EOF
)
  assert_contains "$out" "^e8e7: 1$"

  out=$(run_uci "$ENGINE" "$VARIANTS" jedi-chess <<'EOF'
position fen 8/4q3/8/8/8/8/8/4K3 w - - 0 1
go depth 1
EOF
)
  assert_contains_literal "$out" "bestmove (none)"
}

test_ponder_stop() {
  run_case() {
    local go_cmd="$1"
    run_expect "$ENGINE" <<EOF
$(expect_engine_setup)
   set timeout 10
   send "uci\n"
   expect "uciok"
   send "position startpos\n"
   send "$go_cmd\n"
   after 200
   send "stop\n"
   expect -re {^bestmove\b}
   send "quit\n"
   expect eof
EOF
  }

  run_case "go infinite"
  run_case "go ponder depth 4"
}

test_whaleshogi() {
  local out_start out_promo out_demote
  out_start=$(run_uci "$ENGINE" "$VARIANTS" whaleshogi <<'EOF'
position startpos
go perft 1
EOF
)
  assert_nodes "$out_start" 7

  out_promo=$(run_uci "$ENGINE" "$VARIANTS" whaleshogi <<'EOF'
position fen 5w/4D1/6/6/6/W5 w - - 0 1
go perft 1
EOF
)
  assert_contains_literal "$out_promo" "e5e6+:"
  assert_not_contains_literal "$out_promo" "e5e6:"

  out_demote=$(run_uci "$ENGINE" "$VARIANTS" whaleshogi <<'EOF'
position fen 4+Dw/6/6/6/6/W5 w - - 0 1
go perft 1
EOF
)
  assert_contains_literal "$out_demote" "e6d5-:"
  assert_contains_literal "$out_demote" "e6f5-:"
  assert_not_contains_literal "$out_demote" "e6d5:"
}

test_changing_color_drop_capture
test_dead_pieces
test_material_hand_cache
test_sacrifice
test_stationary_capture
test_self_destruct_special
test_toot_otto
test_hardlimit
test_hindustani
test_jedi_chess
test_ponder_stop
test_whaleshogi
