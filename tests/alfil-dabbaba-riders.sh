#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")/../src"

tmp_ini=$(mktemp)
tmp_key_ini=""
trap 'rm -f "$tmp_ini"' EXIT

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
customPiece1 = a:n{path:mid;filter:any}A2
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 6k1/8/8/8/8/8/1A6/K7 w - - 0 1

[plain-rider-midpoint:chess]
customPiece1 = a:DD
customPiece2 = b:AA
pieceToCharTable = PNBRQ............AB..Kpnbrq............ab..k
startFen = 8/3ab3/3pp3/8/8/8/8/K6k b - - 0 1

[lame-filter-any:chess]
customPiece1 = a:n{path:orthfirst;filter:any}L
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A1p4K w - - 0 1

[lame-filter-mid:chess]
customPiece1 = a:n{path:mid;filter:mid}L
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A1p4K w - - 0 1

[lame-filter-mid-clear:chess]
customPiece1 = a:n{path:mid;filter:mid}L
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A6K w - - 0 1

[moo-anypath:chess]
customPiece1 = a:n{path:anypath;filter:any}N
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/pp6/A6K w - - 0 1

[moa-move-blocked:chess]
customPiece1 = a:n{path:diagfirst;filter:any}N
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/1p5K/A7 w - - 0 1

[lame-filter-first-reject:chess]
customPiece1 = a:n{path:orthfirst;filter:first}L
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A6K w - - 0 1

[lame-filter-last-reject:chess]
customPiece1 = a:n{path:orthfirst;filter:last}L
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A6K w - - 0 1

[lame-invalid-clears-piece:chess]
customPiece1 = a:Rn{path:orthfirst;filter:first}L
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A6K w - - 0 1

[lame-tuple-reject:chess]
customPiece1 = a:n(2,1)
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A6K w - - 0 1

[lame-filter-mid-single:chess]
customPiece1 = a:n{path:mid;filter:mid}D
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/p7/A6K w - - 0 1

[lame-long-leaper:chess]
customPiece1 = a:n{path:anypath;filter:any}U
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A6K w - - 0 1

[moa-check:chess]
customPiece1 = a:n{path:diagfirst;filter:any}N
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 8/8/8/8/3k4/2p5/8/A6K w - - 0 1

[lame-range-reject:chess]
customPiece1 = a:n{path:mid}A[2-3]
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/8/8/A6K w - - 0 1
INI

piece_moves() {
  local variant=$1
  printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value %s\nposition startpos\ngo perft 1\nquit\n' "$tmp_ini" "$variant" \
    | ./stockfish \
    | awk -F: '/^d4/{print $1}' \
    | sort
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

out=$(printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value tuple-range-pin\nposition startpos\ngo perft 1\nquit\n' "$tmp_ini" \
  | ./stockfish)
echo "$out" | grep -q "^d3c3: 1$"
echo "$out" | grep -q "^d3e3: 1$"

# Lame dabbaba/alfil and their rider forms must be blocked by the midpoint square.
out=$(printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value lame-rider-blockers\nposition startpos\ngo perft 1\nquit\n' "$tmp_ini" \
  | ./stockfish)
! echo "$out" | grep -q "^d7d5:"
! echo "$out" | grep -q "^d7f7:"
! echo "$out" | grep -q "^e7c7:"
! echo "$out" | grep -q "^c6e8:"
! echo "$out" | grep -q "^d6f8:"

out=$(printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value lame-rider-repeat\nposition startpos\ngo perft 1\nquit\n' "$tmp_ini" \
  | ./stockfish)
echo "$out" | grep -q "^d7b5: 1$"
echo "$out" | grep -q "^d7f5: 1$"
echo "$out" | grep -q "^d7h3: 1$"

out=$(printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value lame-rider-repeat\nposition fen 8/3a4/8/8/4p3/8/8/K6k b - - 0 1\ngo perft 1\nquit\n' "$tmp_ini" \
  | ./stockfish)
echo "$out" | grep -q "^d7b5: 1$"
echo "$out" | grep -q "^d7f5: 1$"
echo "$out" | grep -q "^d7h3: 1$"

out=$(printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value lame-rider-repeat\nposition fen 8/3a4/8/5p2/8/8/8/K6k b - - 0 1\ngo perft 1\nquit\n' "$tmp_ini" \
  | ./stockfish)
echo "$out" | grep -q "^d7b5: 1$"
! echo "$out" | grep -q "^d7f5:"
! echo "$out" | grep -q "^d7h3:"

out=$(printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value lame-rider-repeat\nposition fen 8/3a4/8/5P2/8/8/8/K6k b - - 0 1\ngo perft 1\nquit\n' "$tmp_ini" \
  | ./stockfish)
echo "$out" | grep -q "^d7b5: 1$"
echo "$out" | grep -q "^d7f5: 1$"
! echo "$out" | grep -q "^d7h3:"

out=$(printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value lame-rider-repeat\nposition fen 8/3a4/8/8/6p1/8/8/K6k b - - 0 1\ngo perft 1\nquit\n' "$tmp_ini" \
  | ./stockfish)
echo "$out" | grep -q "^d7b5: 1$"
echo "$out" | grep -q "^d7f5: 1$"
! echo "$out" | grep -q "^d7h3:"

out=$(printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value lame-rider-bounded\nposition startpos\ngo perft 1\nquit\n' "$tmp_ini" \
  | ./stockfish)
echo "$out" | grep -q "^b2d4: 1$"
echo "$out" | grep -q "^b2f6: 1$"
! echo "$out" | grep -q "^b2h8:"

# Plain DD/AA riders are not lame: midpoint blockers must NOT stop them.
out=$(printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value plain-rider-midpoint\nposition startpos\ngo perft 1\nquit\n' "$tmp_ini" \
  | ./stockfish)
echo "$out" | grep -q "^d7d5:"
echo "$out" | grep -q "^e7c5:"

out=$(printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value lame-filter-any\nposition startpos\ngo perft 1\nquit\n' "$tmp_ini" \
  | ./stockfish)
echo "$out" | grep -q "^a1b4: 1$"

# Midpoint compatibility should still be available for historical definitions.
# For even-length paths, the midpoint region is the whole central segment.
out=$(printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value lame-filter-mid\nposition startpos\ngo perft 1\nquit\n' "$tmp_ini" \
  | ./stockfish)
! echo "$out" | grep -q "^a1d2:"

out=$(printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value lame-filter-mid-clear\nposition startpos\ngo perft 1\nquit\n' "$tmp_ini" \
  | ./stockfish)
echo "$out" | grep -q "^a1d2: 1$"

# Any-path lame knight: if one valid route is clear, the move should be available.
out=$(printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value moo-anypath\nposition startpos\ngo perft 1\nquit\n' "$tmp_ini" \
  | ./stockfish)
echo "$out" | grep -q "^a1c2: 1$"
! echo "$out" | grep -q "^a1e3:"
! echo "$out" | grep -q "^a1g4:"

out=$(printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value moo-anypath\nposition fen k7/8/8/8/8/8/p7/A6K w - - 0 1\ngo perft 1\nquit\n' "$tmp_ini" \
  | ./stockfish)
echo "$out" | grep -q "^a1b3: 1$"

out=$(printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value moa-move-blocked\nposition startpos\ngo perft 1\nquit\n' "$tmp_ini" \
  | ./stockfish)
! echo "$out" | grep -q "^a1c2:"

out=$(printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value moa-check\nposition startpos moves a1c2\nd\nquit\n' "$tmp_ini" \
  | ./stockfish)
echo "$out" | grep -q "Checkers: c2"

out=$(printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value moa-check\nposition fen 8/8/8/8/3k4/3p4/8/A6K w - - 0 1 moves a1c2\nd\nquit\n' "$tmp_ini" \
  | ./stockfish)
! echo "$out" | grep -q "Checkers: c2"

reject_out=$(printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value lame-filter-first-reject\nquit\n' "$tmp_ini" \
  | ./stockfish 2>&1)
grep -q "Unknown Betza lame filter 'first'" <<<"$reject_out"
out=$(printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value lame-filter-first-reject\nposition startpos\ngo perft 1\nquit\n' "$tmp_ini" \
  | ./stockfish)
! echo "$out" | grep -q "^a1"

reject_out=$(printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value lame-filter-last-reject\nquit\n' "$tmp_ini" \
  | ./stockfish 2>&1)
grep -q "Unknown Betza lame filter 'last'" <<<"$reject_out"
out=$(printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value lame-filter-last-reject\nposition startpos\ngo perft 1\nquit\n' "$tmp_ini" \
  | ./stockfish)
! echo "$out" | grep -q "^a1"

out=$(printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value lame-invalid-clears-piece\nposition startpos\ngo perft 1\nquit\n' "$tmp_ini" \
  | ./stockfish)
! echo "$out" | grep -q "^a1"

reject_out=$(printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value lame-tuple-reject\nquit\n' "$tmp_ini" \
  | ./stockfish 2>&1)
grep -q "Unsupported Betza tuple modifier combination" <<<"$reject_out"
out=$(printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value lame-tuple-reject\nposition startpos\ngo perft 1\nquit\n' "$tmp_ini" \
  | ./stockfish)
! echo "$out" | grep -q "^a1"

reject_out=$(printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value lame-range-reject\nquit\n' "$tmp_ini" \
  | ./stockfish 2>&1)
grep -q "Unsupported Betza rider range" <<<"$reject_out"

tmp_key_ini=$(mktemp)
cat > "$tmp_key_ini" <<'INI'
[lame-key-routing:chess]
customPiece1 = a:n{capture:dest;path:orthfirst}L
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k

[hopper-key-routing:chess]
customPiece1 = a:{path:orthfirst;filter:any}W
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
INI

reject_out=$(printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value lame-key-routing\nquit\n' "$tmp_key_ini" \
  | ./stockfish 2>&1)
grep -q "Unknown Betza hopper parameter key 'capture' in lame block" <<<"$reject_out"

reject_out=$(printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value hopper-key-routing\nquit\n' "$tmp_key_ini" \
  | ./stockfish 2>&1)
grep -q "Unknown Betza lame parameter key 'path' in hopper block" <<<"$reject_out"

out=$(printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value lame-filter-mid-single\nposition startpos\ngo perft 1\nquit\n' "$tmp_ini" \
  | ./stockfish)
! echo "$out" | grep -q "^a1a3:"

out=$(printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value lame-long-leaper\nposition startpos\ngo perft 1\nquit\n' "$tmp_ini" \
  | ./stockfish)
echo "$out" | grep -q "^a1e1: 1$"

echo "alfil-dabbaba-riders test OK"
