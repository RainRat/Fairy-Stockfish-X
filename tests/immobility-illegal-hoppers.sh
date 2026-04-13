#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ENGINE="${1:-${SCRIPT_DIR}/../src/stockfish}"

tmp_ini=$(mktemp)
trap 'rm -f "$tmp_ini"' EXIT

cat > "$tmp_ini" <<'INI'
[immobility-illegal-hopper-test:chess]
maxFile = h
maxRank = 8
pieceDrops = true
immobilityIllegal = true
king = k:W
customPiece1 = m:fpR
customPiece2 = g:W
promotedPieceType = m:g
startFen = 8/8/8/8/8/8/8/4K3[M]
INI

out=$(printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value immobility-illegal-hopper-test\nposition fen 8/8/8/8/8/8/8/4K3[M] w - - 0 1\ngo perft 1\nquit\n' "$tmp_ini" | "$ENGINE")

grep -q '^M@a6:' <<<"$out"
grep -q '^M@e6:' <<<"$out"
! grep -q '^M@a7:' <<<"$out"
! grep -q '^M@e7:' <<<"$out"
! grep -q '^M@a8:' <<<"$out"
! grep -q '^M@e8:' <<<"$out"

echo "immobility-illegal hoppers regression passed"
