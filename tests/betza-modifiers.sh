#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")/../src"

tmp_ini=$(mktemp)
trap 'rm -f "$tmp_ini"' EXIT

cat > "$tmp_ini" <<'INI'
[modsugar_contra_group:chess]
customPiece1 = a:o(RB)
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 7k/8/3p4/8/3A1p2/8/3p4/K7 w - - 0 1

[modsugar_contra_explicit:chess]
customPiece1 = a:oRoB
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 7k/8/3p4/8/3A1p2/8/3p4/K7 w - - 0 1

[modsugar_ski_group:chess]
customPiece1 = a:j(RB)
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 4k3/8/8/8/3A4/8/8/4K3 w - - 0 1

[modsugar_ski_explicit:chess]
customPiece1 = a:jRjB
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 4k3/8/8/8/3A4/8/8/4K3 w - - 0 1

[modsugar_max_group:chess]
customPiece1 = a:z(RB)
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 4k3/8/8/8/3A4/8/8/4K3 w - - 0 1

[modsugar_max_explicit:chess]
customPiece1 = a:zRzB
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 4k3/8/8/8/3A4/8/8/4K3 w - - 0 1

[ski_autocheck:chess]
customPiece1 = s:jR
pieceToCharTable = -
startFen = 4k3/4S3/8/8/8/8/8/4K3 w - - 0 1

[dist10:chess]
customPiece1 = a:R10
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 7k/8/8/4A3/8/8/8/K7 w - - 0 1

[tuplewarn:chess]
customPiece1 = a:o(2,1)
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 4k3/8/8/8/3A4/8/8/4K3 w - - 0 1
INI

perft_moves() {
  local variant=$1
  printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value %s\nposition startpos\ngo perft 1\nquit\n' "$tmp_ini" "$variant" \
    | ./stockfish \
    | grep ':'
}

cmp <(perft_moves modsugar_contra_group) <(perft_moves modsugar_contra_explicit)
cmp <(perft_moves modsugar_ski_group) <(perft_moves modsugar_ski_explicit)
cmp <(perft_moves modsugar_max_group) <(perft_moves modsugar_max_explicit)

dist_out=$(printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value dist10\nposition startpos\ngo perft 1\nquit\n' "$tmp_ini" | ./stockfish)
grep -q 'e5e8:' <<<"$dist_out"
grep -q 'e5h5:' <<<"$dist_out"

check_out=$(printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value tuplewarn\nquit\n' "$tmp_ini" | ./stockfish 2>&1)
grep -q "Unsupported Betza tuple modifier combination" <<<"$check_out"

ski_out=$(printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value ski_autocheck\nposition startpos moves e7e5\nd\nquit\n' "$tmp_ini" | ./stockfish)
grep -q 'Checkers: e5 ' <<<"$ski_out"

echo "betza-modifiers test OK"
