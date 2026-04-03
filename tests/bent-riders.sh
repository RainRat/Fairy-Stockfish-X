#!/bin/bash
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
ENGINE="${1:-${REPO_ROOT}/src/stockfish}"
set -euo pipefail

# cd "$(dirname "$0")/../src" # removed for absolute paths

tmp_ini=$(mktemp)
trap 'rm -f "$tmp_ini"' EXIT

cat > "$tmp_ini" <<'INI'
[griffon-test:chess]
customPiece1 = a:O
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 7k/8/8/8/3A4/8/8/K7 w - - 0 1

[manticore-test:chess]
customPiece1 = a:M
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = 7k/8/8/8/3A4/8/8/K7 w - - 0 1
INI

perft_out() {
  local variant="$1"
  printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value %s\nposition startpos\ngo perft 1\nquit\n' "$tmp_ini" "$variant" \
    | "$ENGINE"
}

g=$(perft_out griffon-test)
grep -q "d4h5:" <<<"$g"
grep -q "d4a5:" <<<"$g"
grep -q "d4e8:" <<<"$g"
grep -q "d4c1:" <<<"$g"
! grep -q "d4d5:" <<<"$g"
! grep -q "d4e4:" <<<"$g"

m=$(perft_out manticore-test)
grep -q "d4g8:" <<<"$m"
grep -q "d4a6:" <<<"$m"
grep -q "d4h1:" <<<"$m"
grep -q "d4b1:" <<<"$m"
! grep -q "d4h5:" <<<"$m"
! grep -q "d4e8:" <<<"$m"

echo "bent-riders test OK"