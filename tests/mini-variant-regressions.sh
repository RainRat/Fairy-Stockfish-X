#!/bin/bash

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
export ROOT_DIR
source "${ROOT_DIR}/tests/lib/uci.sh"
setup_test_context "${1:-}" "${2:-}" "mini variant regressions"

test_achi() {
  run_pyffish_test <<'PY'
import os
import pyffish as sf

repo_root = os.environ["ROOT_DIR"]
with open(os.path.join(repo_root, "src", "variants.ini"), encoding="utf-8") as f:
    sf.load_variant_config(f.read())

center = sf.legal_moves("achi", "3/1P1/3 w - - 0 1", [])
assert sorted(center) == sorted(["b2a1", "b2b1", "b2c1", "b2a2", "b2c2", "b2a3", "b2b3", "b2c3"]), center

edge = sf.legal_moves("achi", "3/3/1P1 w - - 0 1", [])
assert sorted(edge) == sorted(["b1a1", "b1c1", "b1b2"]), edge

corner = sf.legal_moves("achi", "3/3/P2 w - - 0 1", [])
assert sorted(corner) == sorted(["a1b1", "a1a2", "a1b2"]), corner

blocked_by_enemy = sf.legal_moves("achi", "3/1Pp/3 w - - 0 1", [])
assert "b2c2" not in blocked_by_enemy, blocked_by_enemy
assert sorted(blocked_by_enemy) == sorted(["b2a1", "b2b1", "b2c1", "b2a2", "b2a3", "b2b3", "b2c3"]), blocked_by_enemy

print("achi regression tests passed")
PY
}

test_rebel_chess() {
  if ! variant_available "$ENGINE" rebel-chess "$VARIANTS"; then
    echo "rebel-chess variant not available in this build; skipping rebel-chess regression"
    return 0
  fi

  local out
  out=$(run_uci "$ENGINE" "$VARIANTS" rebel-chess <<'EOF'
position fen 4s3/8/8/8/8/8/8/4K3 b - - 0 1
go perft 1
EOF
)
  assert_not_contains "$out" "^e8e6: 1$"
  assert_contains "$out" "^e8f6: 1$"

  out=$(run_uci "$ENGINE" "$VARIANTS" rebel-chess <<'EOF'
position fen 3qs3/8/8/8/8/8/8/4K3 b - - 0 1
go perft 1
EOF
)
  assert_contains "$out" "^e8d8: 1$"
}

test_manzhouqi() {
  if ! variant_available "$ENGINE" manzhouqi "$VARIANTS"; then
    echo "manzhouqi variant not available in this build; skipping manzhouqi regression"
    return 0
  fi

  local out
  out=$(run_uci "$ENGINE" "$VARIANTS" manzhouqi <<'EOF'
position startpos
d
EOF
)
  assert_contains_literal "$out" "Fen: rnbakabnr/9/1c5c1/p1p1p1p1p/9/9/P1P1P1P1P/9/9/M1BAKAB2 w - - 0 1"

  out=$(run_uci "$ENGINE" "$VARIANTS" manzhouqi <<'EOF'
position fen 3k5/9/9/9/9/9/9/9/9/M3K4 w - - 0 1
go perft 1
EOF
)
  assert_contains_literal "$out" "a1a5: 1"
  assert_contains_literal "$out" "a1b3: 1"

  out=$(run_uci "$ENGINE" "$VARIANTS" manzhouqi <<'EOF'
position fen 3k5/9/9/9/9/9/p8/9/P8/M3K4 w - - 0 1
go perft 1
EOF
)
  assert_contains_literal "$out" "a1a4: 1"

  out=$(run_uci "$ENGINE" "$VARIANTS" manzhouqi <<'EOF'
position fen 3k5/9/9/9/9/9/9/9/P8/M3K4 w - - 0 1
go perft 1
EOF
)
  assert_not_contains_literal "$out" "a1a4: 1"
}

test_hippolyta() {
  if ! variant_available "$ENGINE" hippolyta "$VARIANTS"; then
    echo "hippolyta variant not available in this build; skipping hippolyta regression"
    return 0
  fi

  local out
  out=$(run_uci "$ENGINE" "$VARIANTS" hippolyta <<'EOF'
position startpos
go perft 1
EOF
)
  assert_contains "$out" "^a1b2: 1$"
  if grep -q "^a1a2:" <<<"$out"; then
    echo "hippolyta generated illegal quiet move"
    return 1
  fi

  out=$(run_uci "$ENGINE" "$VARIANTS" hippolyta <<'EOF'
position startpos moves a1b2
d
EOF
)
  assert_contains_literal "$out" "Fen: aaaaaaaa/AAAAAAAa/AaaaaaAa/AaAAAaAa/AaAaaaAa/AaAAAAAa/A1aaaaaa/AAAAAAAA b - - 0 1"
}

test_atlantis() {
  if ! variant_available "$ENGINE" atlantis "$VARIANTS"; then
    echo "atlantis variant not available in this build; skipping atlantis regression"
    return 0
  fi

  local out
  out=$(run_uci "$ENGINE" "$VARIANTS" atlantis <<'EOF'
position startpos
go perft 1
EOF
)
  assert_contains "$out" "^a2a3: 1$"
  assert_not_contains "$out" "^a2a3,a1: 1$"
  assert_contains "$out" "^0000,a3: 1$"

  out=$(run_uci "$ENGINE" "$VARIANTS" atlantis <<'EOF'
position startpos moves a2a3
d
EOF
)
  assert_contains_literal "$out" "Fen: rnbqkbnr/pppppppp/8/8/8/P7/1PPPPPPP/RNBQKBNR b KQkq - 0 1"

  out=$(run_uci "$ENGINE" "$VARIANTS" atlantis <<'EOF'
position startpos moves 0000,a3
d
EOF
)
  assert_contains_literal "$out" "Fen: rnbqkbnr/pppppppp/8/8/8/*7/PPPPPPPP/RNBQKBNR b KQkq - 1 1"
}

test_saxun() {
  if ! variant_available "$ENGINE" saxun "$VARIANTS"; then
    echo "saxun variant not available in this build; skipping saxun regression"
    return 0
  fi

  local out
  out=$(run_uci "$ENGINE" "$VARIANTS" saxun <<'EOF'
position startpos
d
EOF
)
  assert_contains_literal "$out" "Fen: 1rk4n/nqr2bb1/4pppp/ppp5/5PPP/1PPPP3/P1RQ3N/1NKRBB2 w - - 0 1"

  out=$(run_uci "$ENGINE" "$VARIANTS" saxun <<'EOF'
position fen 8/8/8/8/8/8/P7/8 w - - 0 1
go perft 1
EOF
)
  assert_contains_literal "$out" "a2a3: 1"
  assert_not_contains_literal "$out" "a2a4:"

  out=$(run_uci "$ENGINE" "$VARIANTS" saxun <<'EOF'
position fen 8/P7/8/8/8/8/8/8 w - - 0 1
go perft 1
EOF
)
  assert_not_contains_literal "$out" "a7a8:"
}

test_achi
test_rebel_chess
test_manzhouqi
test_hippolyta
test_atlantis
test_saxun
