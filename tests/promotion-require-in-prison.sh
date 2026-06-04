#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
ENGINE=${1:-"${ROOT_DIR}/src/stockfish"}
if [[ "${ENGINE}" != /* ]]; then
  ENGINE="${PWD}/${ENGINE}"
fi

cd "${ROOT_DIR}/src"

tmp_ini=$(mktemp)
trap 'rm -f "$tmp_ini"' EXIT

cat > "$tmp_ini" <<'INI'
[promprisonok:chess]
pieceDrops = true
captureType = prison
prisonPawnPromotion = true
mandatoryPawnPromotion = true
promotionPieceTypes = q
startFen = 3r3k/6P1/8/8/3Q4/8/8/K7 b - - 0 1

[promprisongate:chess]
pieceDrops = true
captureType = prison
prisonPawnPromotion = true
mandatoryPawnPromotion = true
promotionPieceTypes = q
startFen = 3r3k/6P1/8/8/3Q4/8/8/K7 w - - 0 1
INI

run_cmds() {
  local variant=${2:-promprisonok}
  printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value %s\n%s\nquit\n' "$tmp_ini" "$variant" "$1" | "${ENGINE}"
}

echo "promotion require-in-prison regression tests started"

out=$(run_cmds "position startpos moves d8d4
go perft 1")
grep -q "^g7g8q: 1$" <<<"$out"

out=$(run_cmds "position startpos
position fen 3r3k/6P1/8/8/3Q4/8/8/K7 w - - 0 1
go perft 1" "promprisongate")
! grep -q "^g7g8q:" <<<"$out"

rm -f "$tmp_ini"
trap - EXIT

echo "promotion require-in-prison regression tests passed"
