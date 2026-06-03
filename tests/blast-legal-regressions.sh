#!/bin/bash

set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "blast legal regressions"

load_inline_variants <<'INI'
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

[immobilityblast:chess]
king = -
commoner = k
immobilityIllegal = true
blastOnSameTypeCapture = true
selfCapture = true
mandatoryPawnPromotion = false
startFen = 1P6/P7/8/8/8/8/8/K7 w - - 0 1

[antimatter:chess]
blastOnSameTypeCapture = true
blastOrthogonals = false
blastDiagonals = false

[moverblast:chess]
king = -
commoner = k
blastOnCapture = true
blastOnCaptureMoverCenter = true
blastCenter = false
blastDiagonals = false
startFen = 4k3/8/8/8/8/8/3rp3/4Q2K w - - 0 1

[riflemoverblast:chess]
king = -
commoner = k
rifleCapture = true
blastOnCapture = true
blastOnCaptureMoverCenter = true
blastCenter = false
blastDiagonals = false
startFen = 4k3/8/8/8/8/8/3rp3/4Q2K w - - 0 1

[blastcheck:chess]
checking = false
blastOnCapture = true
blastCenter = true
blastDiagonals = false
# Black king is off the queen's attack line so the evasion only tests the blast.
startFen = 3pr3/8/8/8/8/8/3Q4/k3K3 w - - 0 1
INI
tmp_ini="${FSX_TMP_INI}"

echo "blast legal regressions started"

out=$(run_uci "$ENGINE" "$tmp_ini" blastblock <<'UCI'
position startpos
go perft 1
UCI
)
assert_contains "$out" "^a2e2: 1$"

out=$(run_uci "$ENGINE" "$tmp_ini" selfatomic <<'UCI'
position startpos
go perft 1
UCI
)
assert_not_contains "$out" "^e1e2:"

out=$(run_uci "$ENGINE" "$tmp_ini" immobilityblast <<'UCI'
position startpos
go perft 1
UCI
)
assert_contains "$out" "^a7b8: 1$"

out=$(run_uci "$ENGINE" "$tmp_ini" antimatter <<'UCI'
position startpos moves g2g3
go perft 1
UCI
)
assert_contains "$out" "^Nodes searched: 20$"

# A normal mover-centered capture blasts around the mover's destination.
out=$(run_uci "$ENGINE" "$tmp_ini" moverblast <<'UCI'
position startpos moves e1e2
d
UCI
)
assert_contains "$out" "Fen: 4k3/8/8/8/8/8/4Q3/7K b - - 0 1"

# A rifle mover-centered capture still blasts around the stationary shooter.
out=$(run_uci "$ENGINE" "$tmp_ini" riflemoverblast <<'UCI'
position startpos moves e1e2
d
UCI
)
assert_contains "$out" "Fen: 4k3/8/8/8/8/8/3r4/4Q2K b - - 0 1"

# Capturing the d8 pawn detonates the checking rook on e8, so the move must
# remain legal even though it is not a direct capture of the checker.
out=$(run_uci "$ENGINE" "$tmp_ini" blastcheck <<'UCI'
position startpos
go perft 1
UCI
)
assert_contains "$out" "^d2d8: 1$"

echo "blast legal regressions passed"
