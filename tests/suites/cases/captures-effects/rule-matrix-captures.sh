#!/usr/bin/env bash

set -euo pipefail

source "$(cd "$(dirname "$0")/../../../.." && pwd)/tests/lib/uci.sh"
setup_test_context "${1:-}" "${2:-}" "captures/effects rule matrix"

custom_variants=$(mktemp "${TMPDIR:-/tmp}/fsx-captures-matrix-XXXXXX.ini")
trap 'rm -f "$custom_variants"' EXIT
cat >"$custom_variants" <<'INI'
[blastconnect]
maxRank = 4
maxFile = 4
king = -
immobile = e
wazir = t
customPiece1 = f:mWD
promotedPieceType = e:t t:f f:e
startFen = 4/4/4/4[EEEEEEEEEEEEEeeeeeeeeeeeee] w - - 0 1 {0 0}
stalemateValue = loss
pieceDrops = true
pointsCounting = true
pointsRuleCaptures = owner
piecePoints = e:1 t:1 f:1
blastOnMove = true
blastPromotion = true
blastDiagonals = false
blastCenter = false
removeConnectN = 3
removeConnectNByType = true

[capmapwild:chess]
king = -
customPiece1 = a:W
customPiece2 = b:W
captureForbidden = *:*
captureAllowed = a:b
startFen = 8/8/8/3b4/3A4/8/8/8 w - - 0 1

[selfhouse:crazyhouse]
selfCapture = true

[blast-default-test:fairy]
blastOnCapture = true
rifleCapture = true
king = -
startFen = 8/8/8/8/8/8/8/8 w - - 0 1

[blast-mover-test:fairy]
blastOnCapture = true
rifleCapture = true
blastOnCaptureMoverCenter = true
king = -
startFen = 8/8/8/8/8/8/8/8 w - - 0 1
INI

out=$(run_uci "$ENGINE" "$custom_variants" blastconnect <<'UCI'
position startpos moves E@b3 E@d3 E@c3 E@c2
d
UCI
)
assert_contains_literal "$out" "Fen: 4/4/2e1/4[" "blast promotion removes the connected line"

out=$(run_uci "$ENGINE" "$custom_variants" capmapwild <<'UCI'
position startpos
go perft 1
UCI
)
assert_contains_literal "$out" "d4d5: 1" "explicit capture permission re-enables A to b"

out=$(run_uci "$ENGINE" "$custom_variants" capmapwild <<'UCI'
position fen 8/8/8/3b4/3A4/8/8/8 b - - 0 1
go perft 1
UCI
)
assert_not_contains_literal "$out" "d5d4: 1" "wildcard capture prohibition remains directional"

for capture_case in 'R s d4d5' 'P r d4d5' 'S p d4d5'; do
  read -r attacker victim move <<<"$capture_case"
  fen="9/9/9/9/3${victim}5/3${attacker}5/9/9/9 w - - 0 1"
  out=$(run_uci "$ENGINE" "$VARIANTS" intransitive <<UCI
position fen ${fen}
go perft 1
UCI
)
  assert_contains_literal "$out" "${move}: 1" "intransitive ${attacker}-${victim} capture is allowed"
done

for forbidden_case in 'R p d4d5' 'P s d4d5' 'S r d4d5'; do
  read -r attacker victim move <<<"$forbidden_case"
  fen="9/9/9/9/3${victim}5/3${attacker}5/9/9/9 w - - 0 1"
  out=$(run_uci "$ENGINE" "$VARIANTS" intransitive <<UCI
position fen ${fen}
go perft 1
UCI
)
  assert_not_contains_literal "$out" "${move}: 1" "intransitive ${attacker}-${victim} capture is forbidden"
done

intransitive_start=$(run_uci "$ENGINE" "$VARIANTS" intransitive <<'UCI'
position startpos
d
UCI
)
assert_contains_literal "$intransitive_start" \
  '9/4pr3/4spr2/5spr1/1PS3sp1/1RPS5/2RPS4/3RP4/9 w - - 0 1' \
  "Intransitive starts from the illustrated 9x9 setup with Blue to move"

intransitive_goal=$(run_uci "$ENGINE" "$VARIANTS" intransitive <<'UCI'
position fen 9/7R1/9/9/9/9/9/r8/9 w - - 0 1 moves h8i9
go depth 1
UCI
)
assert_contains_literal "$intransitive_goal" 'info depth 0 score mate 0' \
  "any Intransitive piece reaching the opponent's corner wins"

out=$(run_uci "$ENGINE" "$VARIANTS" capture-anything <<'UCI'
position startpos
go perft 1
UCI
)
assert_contains_literal "$out" "g1e2: 1" "capture-anything permits self-capture"

out=$(run_uci "$ENGINE" "$VARIANTS" capture-anything <<'UCI'
position fen 6k1/8/8/5N2/4P3/8/8/6K1 w - - 17 1 moves e4f5
d
UCI
)
assert_contains_literal "$out" " b - - 0 1" "self-capture resets the halfmove clock"

out=$(run_uci "$ENGINE" "$custom_variants" selfhouse <<'UCI'
position startpos moves g1e2
d
UCI
)
assert_contains_literal "$out" "[P]" "self-capture hand keeps the mover color"

out=$(run_uci "$ENGINE" "$VARIANTS" benedictmorph <<'UCI'
position fen 6k1/8/8/3r4/8/2N5/8/6K1 w - - 0 1 moves c3d5
d
UCI
)
assert_contains_literal "$out" "3R4" "Benedict capture changes the capturer type"

out=$(run_uci "$ENGINE" "$VARIANTS" benedictmorph <<'UCI'
position fen 6k1/8/8/8/8/8/6r1/6K1 w - - 0 1 moves g1g2
d
UCI
)
assert_contains_literal "$out" "6K1" "Benedict capture leaves the king type unchanged"

out=$(run_uci "$ENGINE" "$custom_variants" blast-default-test <<'UCI'
position fen 8/8/3n4/4n3/8/4R3/3n4/8 w - - 0 1 moves e3e5
d
UCI
)
assert_contains_literal "$out" "Fen: 8/8/8/8/8/4R3/3n4/8" "capture blast is centered on the target"

out=$(run_uci "$ENGINE" "$custom_variants" blast-mover-test <<'UCI'
position fen 8/8/3n4/4n3/8/4R3/3n4/8 w - - 0 1 moves e3e5
d
UCI
)
assert_contains_literal "$out" "Fen: 8/8/3n4/8/8/8/8/8" "mover-centered blast removes the shooter"

echo "captures/effects rule matrix cases passed"
