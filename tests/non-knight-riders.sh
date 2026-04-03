#!/bin/bash
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
ENGINE="${1:-${REPO_ROOT}/src/stockfish}"
set -euo pipefail

# cd "$(dirname "$0")/../src" # removed for absolute paths

tmp_ini=$(mktemp)
expected_leaper=$(mktemp)
expected_rider=$(mktemp)
actual_leaper=$(mktemp)
actual_rider_num=$(mktemp)
actual_rider_repeat=$(mktemp)
trap 'rm -f "$tmp_ini" "$expected_leaper" "$expected_rider" "$actual_leaper" "$actual_rider_num" "$actual_rider_repeat"' EXIT

cat > "$tmp_ini" <<'INI'
[camel-leaper:chess]
customPiece1 = a:L
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 7k/8/8/8/4A3/8/8/4K3 w - - 0 1

[camel-rider-num:chess]
customPiece1 = a:L0
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 7k/8/8/8/4A3/8/8/4K3 w - - 0 1

[camel-rider-repeat:chess]
customPiece1 = a:LL
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 7k/8/8/8/4A3/8/8/4K3 w - - 0 1
INI

piece_moves() {
  local variant=$1
  printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value %s\nposition startpos\ngo perft 1\nquit\n' "$tmp_ini" "$variant" \
    | "$ENGINE" \
    | awk -F: '/^e4/{print $1}' \
    | sort
}

cat > "$expected_leaper" <<'EOF'
e4b3
e4b5
e4d1
e4d7
e4f1
e4f7
e4h3
e4h5
EOF

cat > "$expected_rider" <<'EOF'
e4b3
e4b5
e4d1
e4d7
e4f1
e4f7
e4h3
e4h5
EOF

piece_moves camel-leaper > "$actual_leaper"
piece_moves camel-rider-num > "$actual_rider_num"
piece_moves camel-rider-repeat > "$actual_rider_repeat"

cmp "$actual_leaper" "$expected_leaper"
cmp "$actual_rider_num" "$expected_rider"
cmp "$actual_rider_repeat" "$expected_rider"

echo "non-knight-riders test OK"