#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ENGINE="${1:-${SCRIPT_DIR}/../src/stockfish}"

TMP_VARIANTS="$(mktemp /tmp/castling-promoted-piece.XXXXXX.ini)"
trap 'rm -f "$TMP_VARIANTS"' EXIT

cat >"$TMP_VARIANTS" <<'EOF'
[castle-promo-on:chess]
castling = true
castlingPromotedPiece = true
castlingRank = 8
promotionRegionWhite = *8
promotionRegionBlack = *1
promotionPieceTypes = r

[castle-promo-off:castle-promo-on]
castlingPromotedPiece = false
EOF

PROMO_FEN='4K3/P7/8/8/8/8/8/4k3 w - - 0 1'
POST_FEN='R3K3/8/8/8/8/8/8/4k3 b Q - 0 1'
POST_FEN_WHITE='R3K3/8/8/8/8/8/8/4k3 w Q - 0 1'

WITH_RIGHTS="$(
cat <<EOF | "$ENGINE"
setoption name VariantPath value $TMP_VARIANTS
setoption name UCI_Variant value castle-promo-on
position fen $PROMO_FEN moves a7a8r
d
quit
EOF
)"

grep -F "Fen: $POST_FEN" <<<"$WITH_RIGHTS" >/dev/null || {
    echo "expected promoted rook to gain castling rights"
    echo "$WITH_RIGHTS"
    exit 1
}

INCREMENTAL_KEY="$(grep '^Key:' <<<"$WITH_RIGHTS" | awk '{print $2}')"

FRESH="$(
cat <<EOF | "$ENGINE"
setoption name VariantPath value $TMP_VARIANTS
setoption name UCI_Variant value castle-promo-on
position fen $POST_FEN
d
quit
EOF
)"

FRESH_KEY="$(grep '^Key:' <<<"$FRESH" | awk '{print $2}')"

[[ "$INCREMENTAL_KEY" == "$FRESH_KEY" ]] || {
    echo "expected promoted-castling position key to match fresh load"
    echo "incremental key: $INCREMENTAL_KEY"
    echo "fresh key: $FRESH_KEY"
    exit 1
}

WITH_CASTLE="$(
cat <<EOF | "$ENGINE"
setoption name VariantPath value $TMP_VARIANTS
setoption name UCI_Variant value castle-promo-on
position fen $POST_FEN_WHITE
go perft 1
quit
EOF
)"

grep -F "e8c8: 1" <<<"$WITH_CASTLE" >/dev/null || {
    echo "expected castling move after promoted rook gains rights"
    echo "$WITH_CASTLE"
    exit 1
}

WITHOUT_RIGHTS="$(
cat <<EOF | "$ENGINE"
setoption name VariantPath value $TMP_VARIANTS
setoption name UCI_Variant value castle-promo-off
position fen $PROMO_FEN moves a7a8r
d
quit
EOF
)"

grep -F "Fen: R3K3/8/8/8/8/8/8/4k3 b - - 0 1" <<<"$WITHOUT_RIGHTS" >/dev/null || {
    echo "expected control variant to keep castling rights unset"
    echo "$WITHOUT_RIGHTS"
    exit 1
}
