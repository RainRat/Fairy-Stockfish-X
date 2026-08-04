#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/../../../../tests/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "extinction regression"

load_inline_variants <<'INI'
[extinct-any:chess]
extinctionValue = loss
extinctionPieceTypes = qr
extinctionAllPieceTypes = false
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

[extinction-setup-probe:chess]
king = -
checking = false
startFen = 8/8/8/8/8/8/8/8 w - - 0 1
stalemateValue = none
flagPiece = -
extinctionValue = loss
extinctionPieceTypes = *
extinctionMustAppear = *

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

[extinct-all-white-default:chess]
king = -
checking = false
castling = false
pieceToCharTable = -
extinctionValue = loss
extinctionPieceTypes = qr
startFen = 8/8/8/8/8/8/8/8 w - - 0 1

[extinct-any-white-specific:extinct-all-white-default]
extinctionAllPieceTypesWhite = false

[extcount-white-default:chess]
king = -
checking = false
castling = false
pieceToCharTable = -
extinctionValue = loss
extinctionPieceTypes = q
startFen = 8/8/8/8/8/8/8/8 w - - 0 1

[extcount-white-specific:extcount-white-default]
extinctionOpponentPieceCountWhite = 2

[extcount-black-default:chess]
king = -
checking = false
castling = false
pieceToCharTable = -
extinctionValue = loss
extinctionPieceTypes = q
startFen = 8/8/8/8/8/8/8/8 b - - 0 1

[extcount-black-specific:extcount-black-default]
extinctionOpponentPieceCountBlack = 2

[extinct-all-black-default:chess]
king = -
checking = false
castling = false
pieceToCharTable = -
extinctionValue = loss
extinctionPieceTypesWhite = -
extinctionPieceTypesBlack = qr
startFen = 8/8/8/8/8/8/8/8 b - - 0 1

[extinct-any-black-specific:extinct-all-black-default]
extinctionAllPieceTypesBlack = false

[flag-split:chess]
king = -
checking = false
pieceToCharTable = -
flagPieceWhite = q
flagPieceBlack = r
flagRegionWhite = h8
flagRegionBlack = a1
startFen = 8/8/8/8/8/8/8/8 w - - 0 1
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

out=$(run_uci "$ENGINE" "$tmp_ini" extinction-setup-probe <<'UCI'
setoption name Verbosity value 2
position fen 8/8/8/8/8/8/8/8 w - - 0 1
go depth 1
UCI
)
assert_not_contains "$out" "adjudication reason game_end"

out=$(run_uci "$ENGINE" "$tmp_ini" extinction-setup-probe <<'UCI'
setoption name Verbosity value 2
position fen 8/8/8/8/8/8/r7/P7 b - - 0 1 moves a2a1
go depth 1
UCI
)
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

# Sho-Shogi keeps commoner pseudo-royalty separate from the bare-material
# extinction rule.  Capturing one of two commoners must leave the other royal
# piece in play, while capturing the final commoner must end the game even if
# other non-royal material remains.
out=$(run_uci "$ENGINE" "$tmp_ini" shoshogi <<'UCI'
setoption name Verbosity value 2
position fen 4k4/9/9/9/9/9/9/4K4/r3K4 b - - 0 1 moves a1e1
go depth 1
UCI
)
assert_contains "$out" "info string variant shoshogi"
assert_not_contains "$out" "^bestmove \(none\)$"

out=$(run_uci "$ENGINE" "$tmp_ini" shoshogi <<'UCI'
setoption name Verbosity value 2
position fen 4k4/9/9/9/9/9/9/7P/r3K4 b - - 0 1 moves a1e1
go depth 1
UCI
)
assert_contains "$out" "info string adjudication reason game_end"
assert_contains "$out" "^bestmove \(none\)$"

# Baring a lone commoner wins when the opponent has at least three total
# pieces, but the existing extinctionClaim convention preserves the two-piece
# reply/draw exception.
out=$(run_uci "$ENGINE" "$tmp_ini" shoshogi <<'UCI'
setoption name Verbosity value 2
position fen 4k4/9/9/9/9/9/9/rb7/P3K4 b - - 0 1 moves a2a1
go depth 1
UCI
)
assert_contains "$out" "info string adjudication reason game_end"
assert_contains "$out" "^bestmove \(none\)$"

out=$(run_uci "$ENGINE" "$tmp_ini" shoshogi <<'UCI'
setoption name Verbosity value 2
position fen 4k4/9/9/9/9/9/9/r8/P3K4 b - - 0 1 moves a2a1
go depth 1
UCI
)
assert_not_contains "$out" "^bestmove \(none\)$"

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

out=$(run_uci "$ENGINE" "$tmp_ini" extinct-all-white-default <<'UCI'
setoption name Verbosity value 2
setoption name Use NNUE value false
position fen 4Q3/8/8/8/8/8/8/4qr2 w - - 0 1
go depth 1
UCI
)
assert_not_contains "$out" "^bestmove \(none\)$"

out=$(run_uci "$ENGINE" "$tmp_ini" extinct-any-white-specific <<'UCI'
setoption name Verbosity value 2
setoption name Use NNUE value false
position fen 4Q3/8/8/8/8/8/8/4qr2 w - - 0 1
go depth 1
UCI
)
assert_contains "$out" "info string adjudication reason game_end"
assert_contains "$out" "^bestmove \(none\)$"

out=$(run_uci "$ENGINE" "$tmp_ini" extcount-white-default <<'UCI'
setoption name Verbosity value 2
setoption name Use NNUE value false
position fen 4q3/8/8/8/8/8/8/R7 w - - 0 1
go depth 1
UCI
)
assert_contains "$out" "info string adjudication reason game_end"
assert_contains "$out" "^bestmove \(none\)$"

out=$(run_uci "$ENGINE" "$tmp_ini" extcount-white-specific <<'UCI'
setoption name Verbosity value 2
setoption name Use NNUE value false
position fen 4q3/8/8/8/8/8/8/R7 w - - 0 1
go depth 1
UCI
)
assert_not_contains "$out" "^bestmove \(none\)$"

out=$(run_uci "$ENGINE" "$tmp_ini" extcount-black-default <<'UCI'
setoption name Verbosity value 2
setoption name Use NNUE value false
position fen 8/8/8/8/8/8/8/4Q3 b - - 0 1
go depth 1
UCI
)
assert_contains "$out" "info string adjudication reason game_end"
assert_contains "$out" "^bestmove \(none\)$"

out=$(run_uci "$ENGINE" "$tmp_ini" extcount-black-specific <<'UCI'
setoption name Verbosity value 2
setoption name Use NNUE value false
position fen 4Q3/8/8/8/8/8/8/r7 b - - 0 1
go depth 1
UCI
)
assert_not_contains "$out" "^bestmove \(none\)$"

out=$(run_uci "$ENGINE" "$tmp_ini" extinct-all-black-default <<'UCI'
setoption name Verbosity value 2
setoption name Use NNUE value false
position fen 4Q3/8/8/8/8/8/8/4q3 b - - 0 1
go depth 1
UCI
)
assert_not_contains "$out" "^bestmove \(none\)$"

out=$(run_uci "$ENGINE" "$tmp_ini" extinct-any-black-specific <<'UCI'
setoption name Verbosity value 2
setoption name Use NNUE value false
position fen 4Q3/8/8/8/8/8/8/4q3 b - - 0 1
go depth 1
UCI
)
assert_contains "$out" "info string adjudication reason game_end"
assert_contains "$out" "^bestmove \(none\)$"

out=$(run_uci "$ENGINE" "$tmp_ini" flag-split <<'UCI'
setoption name Verbosity value 2
setoption name Use NNUE value false
position fen 4Q3/8/8/8/8/8/8/r7 w - - 0 1
go depth 1
UCI
)
assert_contains "$out" "info string adjudication reason game_end"
assert_contains "$out" "^bestmove \(none\)$"

out=$(run_uci "$ENGINE" "$tmp_ini" flag-split <<'UCI'
setoption name Verbosity value 2
setoption name Use NNUE value false
position fen 7Q/8/8/8/8/8/8/r7 b - - 0 1
go depth 1
UCI
)
assert_contains "$out" "info string adjudication reason game_end"
assert_contains "$out" "^bestmove \(none\)$"

echo "extinction regression tests passed"
