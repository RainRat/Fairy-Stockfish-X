#!/bin/bash

set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "royal variant regressions"

variant_available() {
  local variant="$1"
  probe_variant_available "$ENGINE" "$variant" "$VARIANTS"
}

tmp_dir=$(mktemp -d)

cleanup_tmp_dir() {
  rm -rf "${tmp_dir}"
}

fsx_add_exit_cleanup cleanup_tmp_dir

tmp_ini="${tmp_dir}/royal-regressions.ini"
cat >"${tmp_ini}" <<'INI'
[antiroyal-atomic:atomic]
castling = false
antiRoyalTypes = n
antiRoyalCount = 1

[antiroyal-morph:chess]
antiRoyalTypes = Q
moveMorphPieceType = n:q

[antiroyal-capturemorph:chess]
antiRoyalTypes = Q
captureMorph = true
checking = false

[immune-blast-test:atomic]
blastImmuneTypes = k
pseudoRoyalCaptureIllegal = true

[gate_pseudoroyal_illegal:seirawan]
pseudoRoyalTypes = h
pseudoRoyalCount = 99
castling = false
startFen = 4k3/8/8/8/8/8/b7/1N2K3[H] w B - 0 1

[gate_pseudoroyal_capture:seirawan]
pseudoRoyalTypes = h
pseudoRoyalCount = 99
castling = false
startFen = 4k3/8/8/8/8/8/1b6/2B1K3[H] w C - 0 1

[eppseudo:chess]
customPiece1 = a:W
pseudoRoyalTypes = a
pseudoRoyalCount = 99
blastOnCapture = true
blastCenter = true
blastDiagonals = false
checking = false

[epext:chess]
customPiece1 = a:W
blastOnCapture = true
blastCenter = true
blastDiagonals = false
checking = false
extinctionValue = loss
extinctionPieceTypes = a
extinctionOpponentPieceCount = 1

[blastblock:chess]
blastOnMove = true
blastCenter = false
blastDiagonals = false
startFen = 4r1k1/8/8/8/8/8/R7/K7 w - - 0 1

[selfatomic:chess]
blastOnCapture = true
blastCenter = true
blastDiagonals = true
startFen = 4k3/8/8/8/8/8/4p3/4KQ2 w - - 0 1

[blastcheck:chess]
checking = false
blastOnCapture = true
blastCenter = true
blastDiagonals = false
startFen = 3pr3/8/8/8/8/8/3Q4/k3K3 w - - 0 1

[pairdrop:fairy]
pieceDrops = true
symmetricDropTypes = r
INI

echo "royal variant regressions started"

if variant_available "anti-king-1" && variant_available "anti-king-2"; then
  out=$(run_uci "$ENGINE" "$VARIANTS" anti-king-1 <<'EOF'
position startpos
go perft 1
EOF
)
  assert_contains "$out" "^info string variant anti-king-1 "
  assert_nodes "$out" 20

  out=$(run_uci "$ENGINE" "$VARIANTS" anti-king-2 <<'EOF'
position startpos
go perft 1
EOF
)
  assert_contains "$out" "^info string variant anti-king-2 "
  assert_nodes "$out" 20
  assert_contains "$out" "^d6e6: 1$"
  assert_not_contains "$out" "^d6d7: 1$"

  out=$(run_uci "$ENGINE" "$VARIANTS" anti-king-2 <<'EOF'
position fen 3rr2a/8/8/8/3Ap3/3P4/8/K6R w - - 0 1
go perft 1
EOF
)
  assert_contains "$out" "^d4d3: 1$"
  assert_not_contains "$out" "^d4e4: 1$"
  assert_not_contains "$out" "^h1h8: 1$"

  out=$(run_uci "$ENGINE" "$VARIANTS" anti-king-2 <<'EOF'
position fen 7a/8/8/3Ak3/8/8/8/K6R w - - 0 1
go perft 1
EOF
)
  assert_nodes "$out" 0

  out=$(run_uci "$ENGINE" "$VARIANTS" anti-king-2 <<'EOF'
position fen 3r3a/8/8/3Ak3/8/8/8/K6R w - - 0 1
go perft 1
EOF
)
  assert_nodes "$out" 17
fi

out=$(run_uci "$ENGINE" "$tmp_ini" antiroyal-atomic <<'EOF'
position fen 4r3/8/8/8/8/8/4N3/4k1K1 w - - 0 1
go perft 1
EOF
)
assert_nodes "$out" 0

out=$(run_uci "$ENGINE" "$tmp_ini" antiroyal-atomic <<'EOF'
position fen 4r2k/8/8/8/8/8/4N3/6K1 w - - 0 1
go perft 1
EOF
)
assert_nodes "$out" 5

out=$(run_uci "$ENGINE" "$tmp_ini" antiroyal-morph <<'EOF'
position fen 2q1k2R/8/8/8/8/8/8/3QK1N1 w - - 0 1
go perft 1
EOF
)
assert_contains "$out" "^g1f3: 1$"

out=$(run_uci "$ENGINE" "$tmp_ini" antiroyal-capturemorph <<'EOF'
position fen q3k3/8/8/8/8/8/4q3/2KQ2N1 w - - 0 1
go perft 1
EOF
)
assert_contains "$out" "^g1e2: 1$"

out=$(run_uci "$ENGINE" "$tmp_ini" immune-blast-test <<'EOF'
position fen k1R5/K7/8/8/8/8/8/8 b - - 0 1
go depth 1
EOF
)
assert_contains "$out" "score mate 0"

out=$(run_uci "$ENGINE" "$tmp_ini" gate_pseudoroyal_illegal <<'EOF'
position startpos
go perft 1
EOF
)
assert_contains "$out" "^b1a3: 1$"
assert_not_contains "$out" "^b1a3h: 1$"

out=$(run_uci "$ENGINE" "$tmp_ini" gate_pseudoroyal_capture <<'EOF'
position startpos
go perft 1
EOF
)
assert_contains "$out" "^c1b2: 1$"
assert_not_contains "$out" "^c1b2h: 1$"

out=$(run_uci "$ENGINE" "$tmp_ini" blastblock <<'EOF'
position startpos
go perft 1
EOF
)
assert_contains "$out" "^a2e2: 1$"

out=$(run_uci "$ENGINE" "$tmp_ini" selfatomic <<'EOF'
position startpos
go perft 1
EOF
)
assert_not_contains "$out" "^e1e2:"

out=$(run_uci "$ENGINE" "$tmp_ini" blastcheck <<'EOF'
position startpos
go perft 1
EOF
)
assert_contains "$out" "^d2d8: 1$"

out=$(run_uci "$ENGINE" "$tmp_ini" pairdrop <<'EOF'
position fen 4k3/8/8/8/8/8/8/4K3[RR] w - - 0 1
go perft 1
EOF
)
assert_contains "$out" "^R@a4,h4: 1$"
assert_contains "$out" "^R@d4,e4: 1$"
assert_not_contains "$out" "^R@a4: 1$"

out=$(run_uci "$ENGINE" "$tmp_ini" pairdrop <<'EOF'
position fen 4k3/8/8/8/8/8/8/4K3[RR] w - - 0 1 moves R@a4,h4
d
EOF
)
assert_contains "$out" "Fen: 4k3/8/8/8/R6R/8/8/4K3\\[] b - - 0 1"

echo "royal variant regressions passed"
