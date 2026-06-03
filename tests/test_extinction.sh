#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "extinction regression"

load_inline_variants <<'INI'
[extinct-any:chess]
extinctionValue = loss
extinctionPieceTypes = qr
checking = false
castling = false

[extinct-all:extinct-any]
extinctionAllPieceTypes = true

[test_extinction_check_count]
knight = n
queen = q
king = -
castling = false
extinctionValue = loss
extinctionPieceTypes = *
extinctionPseudoRoyal = true
checkCounting = true
startFen = 4n3/8/8/8/8/8/8/3Q4 w - - 9+9 0 1

[extstal:chess]
extinctionValue = loss
extinctionPieceTypes = p
stalemateValue = loss

[asymext-types:chess]
checking = false
flagPiece = -
extinctionValue = loss
extinctionPieceTypesWhite = q
extinctionPieceTypesBlack = r

[asymext-counts:chess]
checking = false
flagPiece = -
extinctionValue = loss
extinctionPieceTypesWhite = q
extinctionPieceCountWhite = 0
extinctionPieceTypesBlack = r
extinctionPieceCountBlack = 1
INI
tmp_ini="${FSX_TMP_INI}"

assert_game_end() {
  local out="$1"
  assert_contains "$out" "info string adjudication reason game_end"
  assert_contains "$out" "^bestmove \(none\)$"
}

echo "extinction regression tests started"

check_out=$("${ENGINE}" check "${tmp_ini}" 2>&1)
assert_not_contains "$check_out" "invalid configuration"

out=$(run_uci "$ENGINE" "$tmp_ini" extinct-any <<'UCI'
position fen 3qk2r/8/8/8/8/8/4R3/4K3 w - - 0 1
setoption name Verbosity value 2
go depth 1
UCI
)
assert_contains "$out" "info string variant extinct-any"
assert_game_end "$out"

out=$(run_uci "$ENGINE" "$tmp_ini" extinct-all <<'UCI'
position fen 3qk2r/8/8/8/8/8/4R3/4K3 w - - 0 1
setoption name Verbosity value 2
go depth 1
UCI
)
assert_contains "$out" "info string variant extinct-all"
assert_not_contains "$out" "^bestmove \(none\)$"

out=$(run_uci "$ENGINE" "$tmp_ini" extinct-all <<'UCI'
position fen 3qk2r/8/8/8/8/8/8/4K3 w - - 0 1
setoption name Verbosity value 2
go depth 1
UCI
)
assert_contains "$out" "^bestmove \(none\)$"

out=$(run_uci "$ENGINE" "$tmp_ini" test_extinction_check_count <<'UCI'
position startpos moves d1e1
d
UCI
)
assert_contains_literal "$out" "Fen: 4n3/8/8/8/8/8/8/4Q3 b - - 8+9 1 1"

out=$(run_uci "$ENGINE" "$tmp_ini" extstal <<'UCI'
setoption name Verbosity value 2
position fen 7k/5Q2/7K/8/8/8/8/8 b - - 0 1
go depth 1
UCI
)
assert_contains "$out" "info string adjudication reason game_end"
assert_contains "$out" "^bestmove \(none\)$"

out=$(run_uci "$ENGINE" "$tmp_ini" asymext-types <<'UCI'
setoption name Verbosity value 2
position fen 4k2r/8/8/8/8/8/8/4K3 w - - 0 1
go depth 1
UCI
)
assert_game_end "$out"

out=$(run_uci "$ENGINE" "$tmp_ini" asymext-types <<'UCI'
setoption name Verbosity value 2
position fen 4k3/8/8/8/8/8/4Q3/4K3 w - - 0 1
go depth 1
UCI
)
assert_game_end "$out"

out=$(run_uci "$ENGINE" "$tmp_ini" asymext-counts <<'UCI'
setoption name Verbosity value 2
position fen 4k2r/8/8/8/8/8/4Q3/4K3 w - - 0 1
go depth 1
UCI
)
assert_game_end "$out"

out=$(run_uci "$ENGINE" "$tmp_ini" asymext-counts <<'UCI'
setoption name Verbosity value 2
position fen 4k1rr/8/8/8/8/8/8/4KQ2 w - - 0 1
go depth 1
UCI
)
assert_not_contains "$out" "^bestmove \(none\)$"

echo "extinction regression tests passed"
