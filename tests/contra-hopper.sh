#!/bin/bash
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
ENGINE="${1:-${REPO_ROOT}/src/stockfish}"
set -euo pipefail

tmp_ini=$(mktemp)
trap 'rm -f "$tmp_ini"' EXIT

cat > "$tmp_ini" <<'INI'
[contrahopper:chess]
customPiece1 = a:oR
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
startFen = k7/8/8/8/8/5P2/1P3A2/7K w
INI

out=$(printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value contrahopper\nposition startpos\ngo perft 1\nquit\n' "$tmp_ini" | "$ENGINE")

grep -q "f2f4:" <<<"$out"
grep -q "f2f5:" <<<"$out"
grep -q "f2f6:" <<<"$out"
grep -q "f2f7:" <<<"$out"
grep -q "f2f8:" <<<"$out"
! grep -q "f2c2:" <<<"$out"

echo "contra-hopper test OK"
