#!/bin/bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

ENGINE=$(default_engine "${1:-}")

tmp_ini=$(mktemp)
tmp_key_ini=""
trap 'rm -f "$tmp_ini" "$tmp_key_ini"' EXIT

cat > "$tmp_ini" <<'INI'
[alfil-rider:chess]
customPiece1 = a:AA
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 6k1/8/8/8/3A4/8/8/K7 w - - 0 1

[alfil-rider-tuple:chess]
customPiece1 = a:(2,2)(2,2)
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 6k1/8/8/8/3A4/8/8/K7 w - - 0 1

[dabbaba-rider:chess]
customPiece1 = a:DD
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 6k1/8/8/8/3A4/8/8/K7 w - - 0 1

[dabbaba-rider-tuple:chess]
customPiece1 = a:(2,0)2
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 6k1/8/8/8/3A4/8/8/K7 w - - 0 1

[tuple-range-pin:chess]
customPiece1 = a:(1,0)2
customPiece2 = b:W
pieceToCharTable = PNBRQ............AB..Kpnbrq............ab..k
startFen = 3a4/8/8/8/8/3B4/8/3K4 w - - 0 1

[lame-rider-blockers:chess]
customPiece1 = a:nD
customPiece2 = b:nDD
customPiece3 = c:nA
customPiece4 = d:nAA
pieceToCharTable = PNBRQ............ABCDKpnbrq............abcdk
startFen = 8/3ab3/2cd5/8/8/8/8/K6k b - - 0 1

[lame-rider-repeat:chess]
customPiece1 = a:nAA
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 8/3a4/8/8/8/8/8/K6k b - - 0 1

[lame-rider-bounded:chess]
customPiece1 = a:n{path:mid}A2
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 6k1/8/8/8/8/8/1A6/K7 w - - 0 1

[plain-rider-midpoint:chess]
customPiece1 = a:DD
customPiece2 = b:AA
pieceToCharTable = PNBRQ............AB..Kpnbrq............ab..k
startFen = 8/3ab3/3pp3/8/8/8/8/K6k b - - 0 1

[lame-path-orthfirst:chess]
customPiece1 = a:n{path:orthfirst}L
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A1p4K w - - 0 1

[lame-path-mid:chess]
customPiece1 = a:n{path:mid}L
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A1p4K w - - 0 1

[lame-path-mid-clear:chess]
customPiece1 = a:n{path:mid}L
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A6K w - - 0 1

[lame-ferz-blockers:chess]
customPiece1 = a:nF
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 8/8/8/8/3A4/8/8/K6k w - - 0 1

[moo-anypath:chess]
customPiece1 = a:n{path:anypath}N
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/pp6/A6K w - - 0 1

[moa-move-blocked:chess]
customPiece1 = a:n{path:diagfirst}N
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/1p5K/A7 w - - 0 1

[mao-leg-blocked:chess]
customPiece1 = a:n{path:orthfirst}N
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/Ap5K w - - 0 1

[mao-leg-clear:chess]
customPiece1 = a:n{path:orthfirst}N
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A1p4K w - - 0 1

[lame-filter-key-reject:chess]
customPiece1 = a:n{path:orthfirst;filter:first}L
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A6K w - - 0 1

[lame-filter-value-reject:chess]
customPiece1 = a:n{path:orthfirst;filter:last}L
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A6K w - - 0 1

[lame-invalid-clears-piece:chess]
customPiece1 = a:Rn{path:orthfirst;filter:first}L
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A6K w - - 0 1

[lame-invalid-stops-piece:chess]
customPiece1 = a:Rn{path:bad}LB
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A6K w - - 0 1

[lame-invalid-multi-block:chess]
customPiece1 = a:n{path:bad}{path:mid}N
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A6K w - - 0 1

[lame-invalid-dangling-path:chess]
customPiece1 = a:Rn{path:bad}
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A6K w - - 0 1

[lame-invalid-dangling-filter:chess]
customPiece1 = a:Rn{filter:first}
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A6K w - - 0 1

[lame-invalid-only-block:chess]
customPiece1 = a:n{path:bad}
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A6K w - - 0 1

[lame-valid-after-block:chess]
customPiece1 = a:Rn{path:mid}A
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A6K w - - 0 1

[lame-tuple-reject:chess]
customPiece1 = a:Rn(2,1)
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A6K w - - 0 1

[lame-path-mid-single:chess]
customPiece1 = a:n{path:mid}D
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/p7/A6K w - - 0 1

[lame-long-leaper:chess]
customPiece1 = a:n{path:anypath}U
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A6K w - - 0 1

[moa-check:chess]
customPiece1 = a:n{path:diagfirst}N
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 8/8/8/8/3k4/2p5/8/A6K w - - 0 1

[lame-range-reject:chess]
customPiece1 = a:n{path:mid}A[2-3]
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A6K w - - 0 1

[lame-bare-hopper-reject:chess]
customPiece1 = a:npW
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A6K w - - 0 1

[lame-bare-dynamic-reject:chess]
customPiece1 = a:nxR
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A6K w - - 0 1

[lame-bare-ski-reject:chess]
customPiece1 = a:njR
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A6K w - - 0 1

[lame-bare-max-reject:chess]
customPiece1 = a:nzR
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A6K w - - 0 1

[lame-hybrid-rook-check:chess]
customPiece1 = a:RnN
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A6K w - - 0 1

[cylinder-anypath:chess]
cylindrical = true
customPiece1 = a:n{path:anypath}N
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A3K3 w - - 0 1

[cylinder-orthfirst:chess]
cylindrical = true
customPiece1 = a:n{path:orthfirst}N
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A3K3 w - - 0 1
INI

piece_moves() {
  local variant=$1
  run_uci "$ENGINE" "$tmp_ini" "$variant" <<'UCI' | awk -F: '/^d4/{print $1}' | sort
position startpos
go perft 1
UCI
}

expected_alfil=$(mktemp)
expected_dabbaba=$(mktemp)
actual_alfil=$(mktemp)
actual_alfil_tuple=$(mktemp)
actual_dabbaba=$(mktemp)
actual_dabbaba_tuple=$(mktemp)
trap 'rm -f "$tmp_ini" "$tmp_key_ini" "$expected_alfil" "$expected_dabbaba" "$actual_alfil" "$actual_alfil_tuple" "$actual_dabbaba" "$actual_dabbaba_tuple"' EXIT

cat > "$expected_alfil" <<'EOF'
d4b2
d4b6
d4f2
d4f6
d4h8
EOF

cat > "$expected_dabbaba" <<'EOF'
d4b4
d4d2
d4d6
d4d8
d4f4
d4h4
EOF

piece_moves alfil-rider > "$actual_alfil"
piece_moves alfil-rider-tuple > "$actual_alfil_tuple"
piece_moves dabbaba-rider > "$actual_dabbaba"
piece_moves dabbaba-rider-tuple > "$actual_dabbaba_tuple"

cmp "$actual_alfil" "$expected_alfil"
cmp "$actual_alfil_tuple" "$expected_alfil"
cmp "$actual_dabbaba" "$expected_dabbaba"
cmp "$actual_dabbaba_tuple" "$expected_dabbaba"

out=$(run_uci "$ENGINE" "$tmp_ini" tuple-range-pin <<'UCI'
position startpos
go perft 1
UCI
)
assert_contains "$out" "^d3c3: 1$"
assert_contains "$out" "^d3e3: 1$"

# Lame dabbaba/alfil and their rider forms must be blocked by the midpoint square.
out=$(run_uci "$ENGINE" "$tmp_ini" lame-rider-blockers <<'UCI'
position startpos
go perft 1
UCI
)
assert_not_contains "$out" "^d7d5:"
assert_not_contains "$out" "^d7f7:"
assert_not_contains "$out" "^e7c7:"
assert_not_contains "$out" "^c6e8:"
assert_not_contains "$out" "^d6f8:"

out=$(run_uci "$ENGINE" "$tmp_ini" lame-rider-repeat <<'UCI'
position startpos
go perft 1
UCI
)
assert_contains "$out" "^d7b5: 1$"
assert_contains "$out" "^d7f5: 1$"
assert_contains "$out" "^d7h3: 1$"

out=$(run_uci "$ENGINE" "$tmp_ini" lame-rider-repeat <<'UCI'
position fen 8/3a4/8/8/4p3/8/8/K6k b - - 0 1
go perft 1
UCI
)
assert_contains "$out" "^d7b5: 1$"
assert_contains "$out" "^d7f5: 1$"
assert_contains "$out" "^d7h3: 1$"

out=$(run_uci "$ENGINE" "$tmp_ini" lame-rider-repeat <<'UCI'
position fen 8/3a4/8/5p2/8/8/8/K6k b - - 0 1
go perft 1
UCI
)
assert_contains "$out" "^d7b5: 1$"
assert_not_contains "$out" "^d7f5:"
assert_not_contains "$out" "^d7h3:"

out=$(run_uci "$ENGINE" "$tmp_ini" lame-rider-repeat <<'UCI'
position fen 8/3a4/8/5P2/8/8/8/K6k b - - 0 1
go perft 1
UCI
)
assert_contains "$out" "^d7b5: 1$"
assert_contains "$out" "^d7f5: 1$"
assert_not_contains "$out" "^d7h3:"

out=$(run_uci "$ENGINE" "$tmp_ini" lame-rider-repeat <<'UCI'
position fen 8/3a4/8/8/6p1/8/8/K6k b - - 0 1
go perft 1
UCI
)
assert_contains "$out" "^d7b5: 1$"
assert_contains "$out" "^d7f5: 1$"
assert_not_contains "$out" "^d7h3:"

out=$(run_uci "$ENGINE" "$tmp_ini" lame-rider-repeat <<'UCI'
position fen 8/3a4/4p3/8/8/8/8/K6k b - - 0 1
go perft 1
UCI
)
assert_contains "$out" "^d7b5: 1$"
assert_not_contains "$out" "^d7f5:"
assert_not_contains "$out" "^d7h3:"

out=$(run_uci "$ENGINE" "$tmp_ini" lame-rider-bounded <<'UCI'
position startpos
go perft 1
UCI
)
assert_contains "$out" "^b2d4: 1$"
assert_contains "$out" "^b2f6: 1$"
assert_not_contains "$out" "^b2h8:"

# Plain DD/AA riders are not lame: midpoint blockers must NOT stop them.
out=$(run_uci "$ENGINE" "$tmp_ini" plain-rider-midpoint <<'UCI'
position startpos
go perft 1
UCI
)
assert_contains "$out" "^d7d5:"
assert_contains "$out" "^e7c5:"

out=$(run_uci "$ENGINE" "$tmp_ini" lame-path-orthfirst <<'UCI'
position startpos
go perft 1
UCI
)
assert_contains "$out" "^a1b4: 1$"

# Midpoint compatibility should still be available for historical definitions.
# For even-length paths, the midpoint region is the whole central segment.
out=$(run_uci "$ENGINE" "$tmp_ini" lame-path-mid <<'UCI'
position startpos
go perft 1
UCI
)
assert_not_contains "$out" "^a1d2:"

out=$(run_uci "$ENGINE" "$tmp_ini" lame-path-mid-clear <<'UCI'
position startpos
go perft 1
UCI
)
assert_contains "$out" "^a1d2: 1$"

# A lame Ferz is blocked only when both orthogonally adjacent squares are occupied.
out=$(run_uci "$ENGINE" "$tmp_ini" lame-ferz-blockers <<'UCI'
position fen 8/8/8/3p4/3A4/8/8/K6k w - - 0 1
go perft 1
UCI
)
assert_contains "$out" "^d4e5: 1$"

out=$(run_uci "$ENGINE" "$tmp_ini" lame-ferz-blockers <<'UCI'
position fen 8/8/8/3p4/3Ap3/8/8/K6k w - - 0 1
go perft 1
UCI
)
assert_not_contains "$out" "^d4e5:"

# Any-path lame knight: if one valid route is clear, the move should be available.
out=$(run_uci "$ENGINE" "$tmp_ini" moo-anypath <<'UCI'
position startpos
go perft 1
UCI
)
assert_contains "$out" "^a1c2: 1$"
assert_not_contains "$out" "^a1e3:"
assert_not_contains "$out" "^a1g4:"

out=$(run_uci "$ENGINE" "$tmp_ini" moo-anypath <<'UCI'
position fen k7/8/8/8/8/8/p7/A6K w - - 0 1
go perft 1
UCI
)
assert_contains "$out" "^a1b3: 1$"

out=$(run_uci "$ENGINE" "$tmp_ini" moa-move-blocked <<'UCI'
position startpos
go perft 1
UCI
)
assert_not_contains "$out" "^a1c2:"

out=$(run_uci "$ENGINE" "$tmp_ini" mao-leg-blocked <<'UCI'
position startpos
go perft 1
UCI
)
assert_not_contains "$out" "^a1c2:"

out=$(run_uci "$ENGINE" "$tmp_ini" mao-leg-clear <<'UCI'
position startpos
go perft 1
UCI
)
assert_contains "$out" "^a1c2: 1$"

out=$(run_uci "$ENGINE" "$tmp_ini" moa-check <<'UCI'
position startpos moves a1c2
d
UCI
)
assert_contains "$out" "Checkers: c2"

out=$(run_uci "$ENGINE" "$tmp_ini" moa-check <<'UCI'
position fen 8/8/8/8/3k4/3p4/8/A6K w - - 0 1 moves a1c2
d
UCI
)
assert_not_contains "$out" "Checkers: c2"

reject_out=$(run_uci "$ENGINE" "$tmp_ini" lame-filter-key-reject <<'UCI' 2>&1
UCI
)
assert_contains "$reject_out" "Unknown Betza parameter key 'filter' in lame block"
out=$(run_uci "$ENGINE" "$tmp_ini" lame-filter-key-reject <<'UCI'
position startpos
go perft 1
UCI
)
assert_not_contains "$out" "^a1"

reject_out=$(run_uci "$ENGINE" "$tmp_ini" lame-filter-value-reject <<'UCI' 2>&1
UCI
)
assert_contains "$reject_out" "Unknown Betza parameter key 'filter' in lame block"
out=$(run_uci "$ENGINE" "$tmp_ini" lame-filter-value-reject <<'UCI'
position startpos
go perft 1
UCI
)
assert_not_contains "$out" "^a1"

out=$(run_uci "$ENGINE" "$tmp_ini" lame-invalid-clears-piece <<'UCI'
position startpos
go perft 1
UCI
)
assert_not_contains "$out" "^a1"

out=$(run_uci "$ENGINE" "$tmp_ini" lame-invalid-stops-piece <<'UCI'
position startpos
go perft 1
UCI
)
assert_not_contains "$out" "^a1"

out=$(run_uci "$ENGINE" "$tmp_ini" lame-invalid-multi-block <<'UCI'
position startpos
go perft 1
UCI
)
assert_not_contains "$out" "^a1"

for variant in lame-invalid-dangling-path lame-invalid-dangling-filter lame-invalid-only-block; do
  out=$(run_uci "$ENGINE" "$tmp_ini" "$variant" <<'UCI'
position startpos
go perft 1
UCI
)
  assert_not_contains "$out" "^a1"
done

out=$(run_uci "$ENGINE" "$tmp_ini" lame-valid-after-block <<'UCI'
position startpos
go perft 1
UCI
)
assert_contains "$out" "^a1a2: 1$"
assert_contains "$out" "^a1b1: 1$"
assert_contains "$out" "^a1c3: 1$"

reject_out=$(run_uci "$ENGINE" "$tmp_ini" lame-tuple-reject <<'UCI' 2>&1
UCI
)
assert_contains "$reject_out" "Unsupported Betza tuple modifier combination"
out=$(run_uci "$ENGINE" "$tmp_ini" lame-tuple-reject <<'UCI'
position startpos
go perft 1
UCI
)
assert_not_contains "$out" "^a1"

reject_out=$(run_uci "$ENGINE" "$tmp_ini" lame-range-reject <<'UCI' 2>&1
UCI
)
assert_contains "$reject_out" "Unsupported Betza rider range"

for variant in lame-bare-hopper-reject lame-bare-dynamic-reject lame-bare-ski-reject lame-bare-max-reject; do
  reject_out=$(run_uci "$ENGINE" "$tmp_ini" "$variant" <<'UCI' 2>&1
UCI
)
  assert_contains "$reject_out" "Unsupported Betza lame modifier combination"
  out=$(run_uci "$ENGINE" "$tmp_ini" "$variant" <<'UCI'
position startpos
go perft 1
UCI
)
  assert_not_contains "$out" "^a1"
done

out=$(run_uci "$ENGINE" "$tmp_ini" lame-hybrid-rook-check <<'UCI'
position startpos moves a1a7
d
UCI
)
assert_contains "$out" "Checkers: a7"

tmp_key_ini=$(mktemp)
cat > "$tmp_key_ini" <<'INI'
[lame-key-routing:chess]
customPiece1 = a:n{capture:dest;path:orthfirst}L
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k

[hopper-key-routing:chess]
customPiece1 = a:{path:orthfirst}W
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k

[hopper-capture-value-reject:chess]
customPiece1 = a:{capture:bogus}W
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k

[hopper-equi-value-reject:chess]
customPiece1 = a:{equi:bogus}W
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
INI

reject_out=$(run_uci "$ENGINE" "$tmp_key_ini" lame-key-routing <<'UCI' 2>&1
UCI
)
assert_contains "$reject_out" "Unknown Betza parameter key 'capture' in lame block"

reject_out=$(run_uci "$ENGINE" "$tmp_key_ini" hopper-key-routing <<'UCI' 2>&1
UCI
)
assert_contains "$reject_out" "Unknown Betza parameter key 'path' in hopper block"

reject_out=$(run_uci "$ENGINE" "$tmp_key_ini" hopper-capture-value-reject <<'UCI' 2>&1
UCI
)
assert_contains "$reject_out" "Unknown Betza hopper capture mode 'bogus'"
out=$(run_uci "$ENGINE" "$tmp_key_ini" hopper-capture-value-reject <<'UCI'
position startpos
go perft 1
UCI
)
assert_not_contains "$out" "^a1"

reject_out=$(run_uci "$ENGINE" "$tmp_key_ini" hopper-equi-value-reject <<'UCI' 2>&1
UCI
)
assert_contains "$reject_out" "Unknown Betza hopper equi mode 'bogus'"
out=$(run_uci "$ENGINE" "$tmp_key_ini" hopper-equi-value-reject <<'UCI'
position startpos
go perft 1
UCI
)
assert_not_contains "$out" "^a1"

out=$(run_uci "$ENGINE" "$tmp_ini" lame-path-mid-single <<'UCI'
position startpos
go perft 1
UCI
)
assert_not_contains "$out" "^a1a3:"

out=$(run_uci "$ENGINE" "$tmp_ini" lame-long-leaper <<'UCI'
position startpos
go perft 1
UCI
)
assert_contains "$out" "^a1e1: 1$"

# Wrapped-board custom lame profile: ANY_PATH
# clear board
out=$(run_uci "$ENGINE" "$tmp_ini" cylinder-anypath <<'UCI'
position fen k7/8/8/8/8/8/8/A3K3 w - - 0 1
go perft 1
UCI
)
assert_contains "$out" "^a1h3: 1$"
assert_contains "$out" "^a1g2: 1$"
# a2 blocked
out=$(run_uci "$ENGINE" "$tmp_ini" cylinder-anypath <<'UCI'
position fen k7/8/8/8/8/8/P7/A3K3 w - - 0 1
go perft 1
UCI
)
assert_contains "$out" "^a1h3: 1$"
assert_contains "$out" "^a1g2: 1$"
# h2 blocked
out=$(run_uci "$ENGINE" "$tmp_ini" cylinder-anypath <<'UCI'
position fen k7/8/8/8/8/8/7P/A3K3 w - - 0 1
go perft 1
UCI
)
assert_contains "$out" "^a1h3: 1$"
assert_contains "$out" "^a1g2: 1$"
# a2, h2, and h1 blocked: both wrapped routes are gone.
out=$(run_uci "$ENGINE" "$tmp_ini" cylinder-anypath <<'UCI'
position fen k7/8/8/8/8/8/P6P/A3K2P w - - 0 1
go perft 1
UCI
)
assert_not_contains "$out" "^a1h3:"
assert_not_contains "$out" "^a1g2:"

# Wrapped-board custom lame profile: ORTH_FIRST
# clear board
out=$(run_uci "$ENGINE" "$tmp_ini" cylinder-orthfirst <<'UCI'
position fen k7/8/8/8/8/8/8/A3K3 w - - 0 1
go perft 1
UCI
)
assert_contains "$out" "^a1h3: 1$"
assert_contains "$out" "^a1g2: 1$"
# a2 blocked (this leg blocks the ORTH_FIRST wrapped h3 jump)
out=$(run_uci "$ENGINE" "$tmp_ini" cylinder-orthfirst <<'UCI'
position fen k7/8/8/8/8/8/P7/A3K3 w - - 0 1
go perft 1
UCI
)
assert_not_contains "$out" "^a1h3:"
assert_contains "$out" "^a1g2: 1$"

echo "alfil-dabbaba-riders test OK"
