#!/bin/bash

set -euo pipefail

error() {
  echo "asymmetric extinction test failed on line $1" >&2
  exit 1
}
trap 'error ${LINENO}' ERR

python3 - <<'PY'
import pyffish as sf

cfg = """
[asymext-types:chess]
checking = false
flagPiece = -
extinctionValue = loss
extinctionPieceTypesWhite = q
extinctionPieceTypesBlack = r

[asymext-counts:chess]
checking = false
flagPiece = -
extinctionValue = loss
extinctionPieceTypesWhite = q
extinctionPieceCountWhite = 0
extinctionPieceTypesBlack = r
extinctionPieceCountBlack = 1
"""

sf.load_variant_config(cfg)

# White loses when its queen target is gone while black still has its rook target.
white_extinct = "4k2r/8/8/8/8/8/8/4K3 w - - 0 1"
if sf.game_result("asymext-types", white_extinct, []) >= 0:
    raise SystemExit(f"expected white extinction loss: {white_extinct}")

# Black loses when its rook target is gone, even though white still has a queen.
black_extinct = "4k3/8/8/8/8/8/4Q3/4K3 w - - 0 1"
if sf.game_result("asymext-types", black_extinct, []) <= 0:
    raise SystemExit(f"expected black extinction loss: {black_extinct}")

# Color-specific extinction counts differ: black loses already at one rook, white does not lose at one queen.
black_count_extinct = "4k2r/8/8/8/8/8/4Q3/4K3 w - - 0 1"
if sf.game_result("asymext-counts", black_count_extinct, []) <= 0:
    raise SystemExit(f"expected black count-based extinction loss: {black_count_extinct}")

still_alive = "4k1rr/8/8/8/8/8/8/4KQ2 w - - 0 1"
if sf.is_immediate_game_end("asymext-counts", still_alive, [])[0]:
    raise SystemExit(f"did not expect immediate game end: {still_alive}")
PY

echo "asymmetric extinction tests passed"
