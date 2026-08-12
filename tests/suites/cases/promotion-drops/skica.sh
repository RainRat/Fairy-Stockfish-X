#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/../../../../tests/lib/uci.sh"

ENGINE=${1:-${SCRIPT_DIR}/../../../../src/stockfish-large}
VARIANTS=${2:-${SCRIPT_DIR}/../../../../src/variants.ini}

start=$(run_uci "$ENGINE" "$VARIANTS" skica <<'UCI'
position startpos
d
UCI
)
assert_contains_literal "$start" "Fen: s1tcwvct1s/1rnbqkbnr1/pppppppppp/10/10/10/10/PPPPPPPPPP/1RNBQKBNR1/S1TCWVCT1S w KQkq - 0 1" "load Skica's 10x10 setup"

rank8=$(run_uci "$ENGINE" "$VARIANTS" skica <<'UCI'
position fen 9k/10/10/P9/10/10/10/10/5K4/10 w - - 0 1
go perft 1
UCI
)
assert_contains_literal "$rank8" "a7a8w: 1" "allow Wildebeest promotion on rank 8"
assert_not_contains_literal "$rank8" "a7a8q:" "exclude other rank-8 promotion choices"

rank9=$(run_uci "$ENGINE" "$VARIANTS" skica <<'UCI'
position fen 9k/P9/10/10/10/10/10/10/5K4/10 w - - 0 1
go perft 1
UCI
)
assert_contains_literal "$rank9" "a9a10q: 1" "require Queen promotion on rank 10"

rank10=$(run_uci "$ENGINE" "$VARIANTS" skica <<'UCI'
position fen 4k5/10/P9/10/10/10/10/10/5K4/10 w - - 0 1
go perft 1
UCI
)
assert_contains_literal "$rank10" "a8a9v: 1" "allow Ski-Queen promotion on rank 9"

castling=$(run_uci "$ENGINE" "$VARIANTS" skica <<'UCI'
position fen 4k5/10/10/10/10/10/10/10/1R3K2R1/10 w KQ - 0 1
go perft 1
UCI
)
assert_contains_literal "$castling" "f2i2: 1" "support three-square kingside castling"
assert_contains_literal "$castling" "f2c2: 1" "support three-square queenside castling"

echo "Skica promotion and castling cases passed"
