#!/bin/bash
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
ENGINE="${1:-${REPO_ROOT}/src/stockfish}"
set -euo pipefail

# cd "$(dirname "$0")/../src" # removed for absolute paths

tmp_ini=$(mktemp)
trap 'rm -f "$tmp_ini"' EXIT

cat > "$tmp_ini" <<'INI'
[promconsume:chess]
captureType = hand
pieceDrops = true
pocketSize = 5
promotionPieceTypes = q
promotionConsumeInHand = true
startFen = 4k3/6P1/8/8/8/8/8/4K3[Q] w - - 0 1

[promnconsume:chess]
captureType = hand
pieceDrops = true
pocketSize = 5
promotionPieceTypes = q
startFen = 4k3/6P1/8/8/8/8/8/4K3[Q] w - - 0 1

[promconsumeempty:chess]
captureType = hand
pieceDrops = true
pocketSize = 5
promotionPieceTypes = q
promotionConsumeInHand = true
startFen = 4k3/6P1/8/8/8/8/8/4K3[] w - - 0 1
INI

# Consuming mode: promotion legal with [Q], and hand should be consumed after move.
out_consume=$(printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value promconsume\nposition startpos\ngo perft 1\nposition startpos moves g7g8q\nd\nquit\n' "$tmp_ini" | "$ENGINE")
grep -q "g7g8q:" <<<"$out_consume"
grep -q "Fen: 4k1Q~1/8/8/8/8/8/8/4K3\[\] b" <<<"$out_consume"

# Non-consuming mode: same promotion leaves hand intact.
out_nonconsume=$(printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value promnconsume\nposition startpos moves g7g8q\nd\nquit\n' "$tmp_ini" | "$ENGINE")
grep -q "Fen: 4k1Q~1/8/8/8/8/8/8/4K3\[Q\] b" <<<"$out_nonconsume"

# Consuming mode with empty hand: promotion should be disallowed.
out_empty=$(printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value promconsumeempty\nposition startpos\ngo perft 1\nquit\n' "$tmp_ini" | "$ENGINE")
! grep -q "g7g8q:" <<<"$out_empty"

echo "promotionConsumeInHand test OK"