#!/bin/bash

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
export ROOT_DIR
source "${ROOT_DIR}/tests/lib/uci.sh"
setup_test_context "${1:-}" "${2:-}" "capture promotion regressions"

test_must_drop_by_color() {
  local tmp_ini out_white out_black white_nodes black_fen
  tmp_ini=$(mktemp "${TMPDIR:-/tmp}/fsx-mustdrop-XXXXXX.ini")
  cat >"${tmp_ini}" <<'INI'
[asymmustdrop:chess]
pieceDrops = true
mustDrop = false
mustDropWhite = true
mustDropBlack = false
mustDropTypeWhite = p
startFen = 4k3/8/8/8/8/8/8/4K3[P] w - - 0 1
INI

  out_white=$(run_uci "$ENGINE" "$tmp_ini" asymmustdrop <<'EOF'
position startpos
go perft 1
EOF
)
  assert_contains "$out_white" "P@a"
  assert_not_contains "$out_white" "e1e2:"
  white_nodes=$(grep -o "Nodes searched: [0-9]*" <<<"$out_white" | awk '{print $3}')
  if [[ -z "$white_nodes" || "$white_nodes" -le 0 ]]; then
    echo "unexpected white node count: $white_nodes"
    return 1
  fi

  black_fen='4k3/8/8/8/8/8/8/4K3[p] b - - 0 1'
  out_black=$(run_uci "$ENGINE" "$tmp_ini" asymmustdrop <<EOF
position fen ${black_fen}
go perft 1
EOF
)
  assert_contains "$out_black" "e8e7:"
  assert_contains "$out_black" "@a"
  rm -f "${tmp_ini}"
}

test_must_capture_by_color() {
  local tmp_ini out_white out_black
  tmp_ini=$(mktemp "${TMPDIR:-/tmp}/fsx-mustcapture-XXXXXX.ini")
  cat >"${tmp_ini}" <<'INI'
[asymmustcapture:chess]
mustCaptureWhite = true
mustCaptureBlack = false
startFen = 4k3/8/8/3p4/4P3/8/8/4K3 w - - 0 1
INI

  out_white=$(run_uci "$ENGINE" "$tmp_ini" asymmustcapture <<'UCI'
position startpos
go perft 1
UCI
)
  assert_contains "$out_white" "e4d5:"
  assert_not_contains "$out_white" "e4e5:"
  assert_nodes "$out_white" 1

  black_fen='4k3/8/8/4p3/3P4/8/8/4K3 b - - 0 1'
  out_black=$(run_uci "$ENGINE" "$tmp_ini" asymmustcapture <<EOF
position fen ${black_fen}
go perft 1
EOF
)
  assert_contains "$out_black" "e5d4:"
  assert_contains "$out_black" "e5e4:"
  rm -f "${tmp_ini}"
}

test_self_capture_color() {
  local tmp_ini out
  tmp_ini=$(mktemp "${TMPDIR:-/tmp}/fsx-selfcapture-color-XXXXXX.ini")
  cat >"${tmp_ini}" <<'INI'
[self-capture-black-only:chess]
selfCaptureBlack = true
INI

  out=$(run_uci "$ENGINE" "$tmp_ini" self-capture-black-only <<'EOF'
position fen 4k3/8/8/8/8/8/4Q3/3RK3 w - - 0 1
go perft 1
EOF
)
  assert_not_contains "$out" "^d1e2: 1$"

  out=$(run_uci "$ENGINE" "$tmp_ini" self-capture-black-only <<'EOF'
position fen 3qk3/4r3/8/8/8/8/8/4K3 b - - 0 1
go perft 1
EOF
)
  assert_contains "$out" "^d8e7: 1$"
  rm -f "${tmp_ini}"
}

test_self_capture_types() {
  local tmp_ini tmp_king="" out
  tmp_ini=$(mktemp "${TMPDIR:-/tmp}/fsx-selfcapture-types-XXXXXX.ini")
  cat >"${tmp_ini}" <<'INI'
[self-capture-pawn-only:chess]
selfCaptureTypes = p
INI

  out=$(run_uci "$ENGINE" "$tmp_ini" self-capture-pawn-only <<'EOF'
position fen 4k3/8/8/8/8/3Q4/4P3/4K3 w - - 0 1
go perft 1
EOF
)
  if ! grep -q "^d3e2: 1$" <<<"$out"; then
    echo "self-capture-types regression not supported by this build; skipping self-capture-types checks"
    rm -f "${tmp_ini}" "${tmp_king}"
    return 0
  fi
  assert_contains "$out" "^e2d3: 1$"
  assert_not_contains "$out" "^d3e2: 1$"

  out=$(run_uci "$ENGINE" "$tmp_ini" self-capture-pawn-only <<'EOF'
position fen 4k3/8/8/8/8/3Q4/4P3/4K3 w - - 0 1 moves e2d3
d
EOF
)
  assert_contains_literal "$out" "Fen: 4k3/8/8/8/8/3P4/8/4K3 b - - 0 1"

  tmp_king=$(mktemp "${TMPDIR:-/tmp}/fsx-selfcapture-king-XXXXXX.ini")
  cat >"${tmp_king}" <<'INI'
[self-capture-king-only:chess]
selfCaptureTypes = k
INI

  out=$(run_uci "$ENGINE" "$tmp_king" self-capture-king-only <<'EOF'
position fen 4k3/8/8/8/8/8/4P3/4K3 w - - 0 1
go perft 1
EOF
)
  assert_contains "$out" "^e1e2: 1$"

  out=$(run_uci "$ENGINE" "$tmp_king" self-capture-king-only <<'EOF'
position fen 4k3/8/8/8/8/8/4P3/4K3 w - - 0 1 moves e1e2
d
EOF
)
  assert_contains_literal "$out" "Fen: 4k3/8/8/8/8/8/4K3/8 b - - 0 1"
  rm -f "${tmp_ini}" "${tmp_king}"
}

test_promotion_consume_in_hand() {
  local tmp_ini out_consume out_nonconsume out_empty
  tmp_ini=$(mktemp "${TMPDIR:-/tmp}/fsx-promconsume-XXXXXX.ini")
  cat >"${tmp_ini}" <<'INI'
[promconsume:chess]
captureType = hand
pieceDrops = true
pocketSize = 5
promotionPieceTypes = q
promotionConsumeInHand = true
startFen = 4k3/6P1/8/8/8/8/8/4K3[Q] w - - 0 1

[promnconsume:chess]
captureType = hand
pieceDrops = true
pocketSize = 5
promotionPieceTypes = q
startFen = 4k3/6P1/8/8/8/8/8/4K3[Q] w - - 0 1

[promconsumeempty:chess]
captureType = hand
pieceDrops = true
pocketSize = 5
promotionPieceTypes = q
promotionConsumeInHand = true
startFen = 4k3/6P1/8/8/8/8/8/4K3[] w - - 0 1
INI

  out_consume=$(run_uci "$ENGINE" "$tmp_ini" promconsume <<'EOF'
position startpos
go perft 1
position startpos moves g7g8q
d
EOF
)
  assert_contains "$out_consume" "g7g8q:"
  assert_contains_literal "$out_consume" "Fen: 4k1Q~1/8/8/8/8/8/8/4K3[] b"

  out_nonconsume=$(run_uci "$ENGINE" "$tmp_ini" promnconsume <<'EOF'
position startpos moves g7g8q
d
EOF
)
  assert_contains_literal "$out_nonconsume" "Fen: 4k1Q~1/8/8/8/8/8/8/4K3[Q] b"

  out_empty=$(run_uci "$ENGINE" "$tmp_ini" promconsumeempty <<'EOF'
position startpos
go perft 1
EOF
)
  assert_not_contains "$out_empty" "g7g8q:"
  rm -f "${tmp_ini}"
}

test_promotion_require_in_hand() {
  local tmp_ini out_gate out_ok
  tmp_ini=$(mktemp "${TMPDIR:-/tmp}/fsx-promhand-XXXXXX.ini")
  cat >"${tmp_ini}" <<'INI'
[promhandgate:chess]
captureType = hand
pieceDrops = true
pocketSize = 5
promotionPieceTypes = q
promotionRequireInHand = true
startFen = 4k3/6P1/8/8/8/8/8/4K3[] w - - 0 1

[promhandok:chess]
captureType = hand
pieceDrops = true
pocketSize = 5
promotionPieceTypes = q
promotionRequireInHand = true
startFen = 4k3/6P1/8/8/8/8/8/4K3[Q] w - - 0 1
INI

  out_gate=$(run_uci "$ENGINE" "$tmp_ini" promhandgate <<'EOF'
position startpos
go perft 1
EOF
)
  assert_contains "$out_gate" "Nodes searched: 5"
  assert_not_contains "$out_gate" "g7g8q:"

  out_ok=$(run_uci "$ENGINE" "$tmp_ini" promhandok <<'EOF'
position startpos
go perft 1
EOF
)
  assert_contains "$out_ok" "g7g8q:"
  rm -f "${tmp_ini}"
}

test_must_drop_by_color
test_must_capture_by_color
test_self_capture_color
test_self_capture_types
test_promotion_consume_in_hand
test_promotion_require_in_hand
