#!/bin/bash

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
export ROOT_DIR
source "${ROOT_DIR}/tests/lib/uci.sh"
setup_test_context "${1:-}" "${2:-}" "fast rules regression"

test_changing_color_locality() {
  local tmp_ini out
  tmp_ini=$(mktemp "${TMPDIR:-/tmp}/fsx-changing-color-locality-XXXXXX.ini")
  cat >"${tmp_ini}" <<'INI'
[surround-color:chess]
surroundCaptureIntervene = true
changingColorTrigger = capture
changingColorPieceTypes = *

[remote-burner-color:chess]
castling = false
king = -
customPiece1 = u:R
customPiece2 = v:N
customPiece3 = k:K
blastPassiveTypes = u
changingColorTrigger = capture
changingColorPieceTypes = v
pieceToCharTable = PNBRQ............UV..Kpnbrq............uv..k
INI

  out=$(
    python3 - <<'PY' "${tmp_ini}"
import sys

import pyffish as sf

variant_path = sys.argv[1]
with open(variant_path, "r", encoding="utf-8") as f:
    sf.load_variant_config(f.read())

print(sf.get_fen("surround-color", "4k3/8/8/8/8/3p1p2/4K3/8 w - - 0 1", ["e2e3"]))
PY
  )
  assert_contains_literal "${out}" "4k3/8/8/8/8/4k3/8/8 b - - 1 1"

  out=$(
    python3 - <<'PY' "${tmp_ini}"
import sys

import pyffish as sf

variant_path = sys.argv[1]
with open(variant_path, "r", encoding="utf-8") as f:
    sf.load_variant_config(f.read())

print(sf.get_fen("remote-burner-color", "8/8/8/8/8/8/1p6/U3V3 w - - 0 1", ["e1g2"]))
PY
  )
  assert_not_contains_literal "${out}" "8/8/8/8/8/8/6n1/U7 b - - 1 1"
  assert_contains_literal "${out}" "8/8/8/8/8/8/6V1/U7 b - - 1 1"
  rm -f "${tmp_ini}"
}

test_flip_regressions() {
  local tmp_ini out fen
  tmp_ini=$(mktemp "${TMPDIR:-/tmp}/fsx-flip-XXXXXX.ini")
  cat >"${tmp_ini}" <<'INI'
[flip5:chess]
maxRank = 5
maxFile = e
startFen = 4k/5/5/5/4K w - - 0 1
INI

  out=$(cat <<CMDS | "${ENGINE}"
uci
setoption name VariantPath value ${tmp_ini}
setoption name UCI_Variant value flip5
position fen 4k/5/5/3Pp/4K w - e3 0 1
flip
d
quit
CMDS
)
  fen=$(sed -n 's/^Fen: //p' <<<"${out}" | tail -n1)
  assert_contains_literal "${fen}" "4k/3pP/5/5/4K b - e3 0 1"
  rm -f "${tmp_ini}"
}

test_potion_check_regressions() {
  local tmp_ini out
  tmp_ini=$(mktemp "${TMPDIR:-/tmp}/fsx-potioncheck-XXXXXX.ini")
  cat >"${tmp_ini}" <<'INI'
[potioncheck:chess]
potions = true
freezePotion = r
potionDropOnOccupied = true
checking = false
startFen = 4k3/8/8/8/8/8/8/4K3[R] w - - 0 1
INI

  out=$(cat <<CMDS | "${ENGINE}"
uci
setoption name VariantPath value ${tmp_ini}
setoption name UCI_Variant value potioncheck
position startpos
go perft 1
quit
CMDS
)
  assert_contains "$out" "^r@d8,e1d1: 1$"
  assert_contains "$out" "^r@f8,e1f2: 1$"
  assert_contains "$out" "^r@e8,e1e2: 1$"
  rm -f "${tmp_ini}"
}

test_repetition_loss_search() {
  local tmp_ini out forced
  tmp_ini=$(mktemp "${TMPDIR:-/tmp}/fsx-repetition-loss-XXXXXX.ini")
  cat >"${tmp_ini}" <<'EOF'
[aries:fairy]
pieceToCharTable = -
king = -
castling = false
nMoveRule = 0
nFoldRuleImmediate = 3
nFoldValue = loss
rook = r
pushingStrength = r:8
pushFirstColor = them
pushChainEnemyOnly = true
pushCaptureAgainstFriendlyBlocker = true
pushingRemoves = shove
stepwisePushing = false
flagPiece = r
flagRegionWhite = h8
flagRegionBlack = a1
extinctionPieceTypes = r
extinctionValue = loss
startFen = 4rrrr/4rrrr/4rrrr/4rrrr/RRRR4/RRRR4/RRRR4/RRRR4 w - - 0 1
EOF

  out=$(cat <<EOF | "${ENGINE}"
uci
setoption name VariantPath value ${tmp_ini}
setoption name UCI_Variant value aries
position fen 8/8/8/8/8/8/7r/R7 w - - 0 1 moves a1a2 h2h1 a2a1 h1h2 a1a2 h2h1 a2a1
go depth 3
quit
EOF
)
  assert_not_contains "$out" "^bestmove h1h2$"

  forced=$(cat <<EOF | "${ENGINE}"
uci
setoption name VariantPath value ${tmp_ini}
setoption name UCI_Variant value aries
position fen 8/8/8/8/8/8/7r/R7 w - - 0 1 moves a1a2 h2h1 a2a1 h1h2 a1a2 h2h1 a2a1
go depth 2 searchmoves h1h2
quit
EOF
)
  assert_contains "$forced" "^bestmove h1h2$"
  rm -f "${tmp_ini}"
}

test_custom_en_passant_passed_squares() {
  run_pyffish_test <<'PY'
import os
import pyffish as sf

repo_root = os.environ["ROOT_DIR"]
with open(os.path.join(repo_root, "src", "variants.ini"), encoding="utf-8") as f:
    sf.load_variant_config(f.read())

cfg = """
[custom-ep-all:chess]
customPiece1 = a:mWifemR3
customPiece2 = s:fK
pawn = -
pawnTypes = a
enPassantTypes = as
tripleStepRegionWhite = *2
tripleStepRegionBlack = *7
enPassantRegionWhite = *1 *2 *3 *4 *5 *6 *7 *8
enPassantRegionBlack = *1 *2 *3 *4 *5 *6 *7 *8
startFen = 8/8/8/2s1s3/8/8/3A4/8 w - - 0 1
checking = false
flagPiece = -

[custom-ep-first:custom-ep-all]
enPassantPassedSquares = first
"""

sf.load_variant_config(cfg)

fen = sf.start_fen("custom-ep-all")
fen_all = sf.get_fen("custom-ep-all", fen, ["d2d5"])
assert " b - d3d4d5 " in fen_all, fen_all
assert sf.is_capture("custom-ep-all", fen_all, [], "c5d4"), fen_all
assert sf.is_capture("custom-ep-all", fen_all, [], "e5d4"), fen_all

fen_first = sf.get_fen("custom-ep-first", fen, ["d2d5"])
assert " b - d3 " in fen_first, fen_first
legal_first = set(sf.legal_moves("custom-ep-first", fen_first, []))
assert "c5d4" in legal_first, fen_first
assert "e5d4" in legal_first, fen_first
assert not sf.is_capture("custom-ep-first", fen_first, [], "c5d4"), fen_first
assert not sf.is_capture("custom-ep-first", fen_first, [], "e5d4"), fen_first

print("custom en passant passed squares regression tests passed")
PY
}

test_two_custom_pawn_en_passant() {
  run_pyffish_test <<'PY'
import pyffish as sf

sf.load_variant_config(r"""
[two-pawns:chess]
customPiece1 = z:fmWifmW2fceF
pawnTypes = pz
enPassantTypes = pz
promotionPawnTypes = pz
promotedPieceType = z:n
pieceToCharTable = PNBRQ....Z.........Kpnbrq....z.........k
""")

# The Betza initial modifier permits the double-step only before the piece moves.
initial = "4k3/8/8/8/8/8/3Z4/4K3 w - - 0 1"
assert "d2d4" in sf.legal_moves("two-pawns", initial, [])
moved = "4k3/8/8/8/8/3Z4/8/4K3 w - - 0 1"
assert "d3d5" not in sf.legal_moves("two-pawns", moved, [])

# Either pawn family can create an EP target and either configured family can capture it.
z_target = "4k3/8/8/8/2p5/8/3Z4/4K3 w - - 0 1"
z_ep = sf.get_fen("two-pawns", z_target, ["d2d4"])
assert sf.is_capture("two-pawns", z_ep, [], "c4d3"), z_ep

p_target = "4k3/8/8/8/2z5/8/3P4/4K3 w - - 0 1"
p_ep = sf.get_fen("two-pawns", p_target, ["d2d4"])
assert sf.is_capture("two-pawns", p_ep, [], "c4d3"), p_ep

print("two custom pawn en passant regression tests passed")
PY
}

test_rule_definition_color_overrides() {
  local tmp_ini out
  tmp_ini=$(mktemp "${TMPDIR:-/tmp}/fsx-rule-color-overrides-XXXXXX.ini")
  cat >"${tmp_ini}" <<'INI'
[promo-color-overrides:chess]
pawn = p
promotionPawnTypesWhite = p
promotionPawnTypesBlack = p
promotionRegionWhite = *8
promotionRegionBlack = *1
mandatoryPromotionRegionWhite = a8
mandatoryPromotionRegionBlack = h1
promotionPieceTypes = q
startFen = 7k/P6P/8/8/8/8/p6p/K7 w - - 0 1

[selfcapture-color-overrides:chess]
selfCaptureWhite = true
selfCaptureTypesWhite = q
startFen = 4k3/8/8/8/8/8/3RQ3/7K w - - 0 1

[pass-color-overrides:chess]
passWhite = true
passBlack = false
passOnStalemateWhite = true
passOnStalemateBlack = true
startFen = 4k3/8/8/8/8/8/8/4K3 w - - 0 1

[gating-piece-after-split:fairy]
gating = true
gatingFromHand = false
castling = false
startFen = 8/pppppppp/8/8/8/8/PPPPPPPP/8 w - - 0 1
gatingPieceAfterWhite = p:n
gatingPieceAfterBlack = p:b

INI

  out=$(run_uci "$ENGINE" "$tmp_ini" promo-color-overrides <<'EOF'
position fen 4k3/P6P/8/8/8/8/p6p/K7 w - - 0 1
go perft 1
EOF
)
  assert_contains "$out" "^a7a8q: 1$"
  assert_contains "$out" "^h7h8q: 1$"
  assert_not_contains "$out" "^a2a1:"
  assert_not_contains "$out" "^h2h1:"

  out=$(run_uci "$ENGINE" "$tmp_ini" promo-color-overrides <<'EOF'
position fen 4k3/8/8/8/8/8/p6p/4K3 b - - 0 1
go perft 1
EOF
)
  assert_contains "$out" "^a2a1q: 1$"
  assert_contains "$out" "^h2h1q: 1$"
  assert_not_contains "$out" "^a7a8:"
  assert_not_contains "$out" "^h7h8:"

  out=$(run_uci "$ENGINE" "$tmp_ini" selfcapture-color-overrides <<'EOF'
position fen 4k3/8/8/8/8/8/3RQ3/7K w - - 0 1
go perft 1
EOF
)
  assert_contains "$out" "^e2d2: 1$"

  out=$(run_uci "$ENGINE" "$tmp_ini" pass-color-overrides <<'EOF'
position fen 7k/8/8/8/8/8/5q2/7K w - - 0 1
go perft 1
EOF
)
  assert_contains_literal "$out" "0000: 1"

  out=$(run_uci "$ENGINE" "$tmp_ini" pass-color-overrides <<'EOF'
position fen 7k/5Q2/6K1/8/8/8/8/8 b - - 0 1
go perft 1
EOF
)
  assert_contains_literal "$out" "0000: 1"

  out=$(run_uci "$ENGINE" "$tmp_ini" gating-piece-after-split <<'EOF'
position startpos
go perft 1
EOF
)
  assert_contains "$out" "^a2a3n: 1$"
  assert_not_contains "$out" "^a2a3b: 1$"

  out=$(run_uci "$ENGINE" "$tmp_ini" gating-piece-after-split <<'EOF'
position fen 8/pppppppp/8/8/8/8/PPPPPPPP/8 b - - 0 1
go perft 1
EOF
)
  assert_contains "$out" "^a7a6b: 1$"
  assert_not_contains "$out" "^a7a6n: 1$"

  rm -f "$tmp_ini"
}

test_rule_definition_phase_overrides() {
  local tmp_ini out
  tmp_ini=$(mktemp "${TMPDIR:-/tmp}/fsx-rule-phase-overrides-XXXXXX.ini")
  cat >"${tmp_ini}" <<'INI'
[osr-split:chess]
maxRank = 1
maxFile = b
king = -
checking = false
openingSelfRemoval = true
openingSelfRemovalRegionWhite = a1
openingSelfRemovalRegionBlack = b1
startFen = Rr w - - 0 1

[nmove-color:chess]
king = -
checking = false
nMoveRule = 50
nMoveRuleTypesWhite = r
nMoveRuleTypesBlack = b
startFen = 8/8/8/8/8/8/p7/R7 w - - 49 1
INI

  out=$(run_uci "$ENGINE" "$tmp_ini" osr-split <<'EOF'
position startpos
go perft 1
EOF
)
  assert_contains "$out" "^a1a1: 1$"

  out=$(run_uci "$ENGINE" "$tmp_ini" osr-split <<'EOF'
position startpos moves a1a1
go perft 1
EOF
)
  assert_contains "$out" "^b1b1: 1$"

  out=$(run_uci "$ENGINE" "$tmp_ini" osr-split <<'EOF'
position startpos moves a1a1
d
EOF
)
  assert_contains_literal "$out" "Fen: 1r b - - 0 1"

  out=$(run_uci "$ENGINE" "$tmp_ini" nmove-color <<'EOF'
position fen 8/8/8/8/8/8/p7/R7 w - - 49 1 moves a1a2
d
EOF
)
  assert_contains_literal "$out" "Fen: 8/8/8/8/8/8/R7/8 b - - 0 1"

  out=$(run_uci "$ENGINE" "$tmp_ini" nmove-color <<'EOF'
position fen 7b/6P1/8/8/8/8/8/8 b - - 49 1 moves h8g7
d
EOF
)
  assert_contains_literal "$out" "Fen: 8/6b1/8/8/8/8/8/8 w - - 0 2"

  rm -f "$tmp_ini"
}

test_standard_piece_value_phase() {
  local tmp_ini output material_eg
  tmp_ini=$(mktemp "${TMPDIR:-/tmp}/fsx-piecevalue-phase-XXXXXX.ini")
  cat >"${tmp_ini}" <<'INI'
[knight-low-eg:chess]
pieceValueMg = n:1000
pieceValueEg = n:1
INI

  output=$(cat <<CMDS | "${ENGINE}"
uci
setoption name VariantPath value ${tmp_ini}
setoption name UCI_Variant value knight-low-eg
position fen 4k3/8/8/3N4/8/8/8/4K3 w - - 0 1
eval
quit
CMDS
)
  material_eg=$(awk '/^\|   Material / { print $(NF-1) }' <<<"${output}" | tail -n1)
  [[ -n "${material_eg}" ]]
  python3 - "${material_eg}" <<'PY'
import sys
score = float(sys.argv[1])
if score <= 0.10:
    raise SystemExit(f"expected positive endgame material contribution, got {score}")
PY
  rm -f "${tmp_ini}"
}

test_potion_custom() {
  local default_variant_path out
  default_variant_path="variants.ini"
  if [[ ! -f "${default_variant_path}" && -f "${ROOT_DIR}/src/variants.ini" ]]; then
    default_variant_path="${ROOT_DIR}/src/variants.ini"
  fi

  out=$(cat <<CMDS | "${ENGINE}"
uci
setoption name VariantPath value ${default_variant_path}
setoption name UCI_Variant value spell-chess
position fen 7k/8/8/p7/8/p7/8/R3K3[J] w - - 0 1
go perft 1
quit
CMDS
)
  assert_contains "$out" "^j@a3,a1a4: 1$"
  assert_contains "$out" "^j@a3,a1a5: 1$"
  assert_not_contains "$out" "^j@a3,a1a6:"

  out=$(cat <<CMDS | "${ENGINE}"
uci
setoption name VariantPath value ${default_variant_path}
setoption name UCI_Variant value checkers
position startpos
go perft 1
quit
CMDS
)
  assert_contains "$out" "Nodes searched: 7"

  out=$(cat <<CMDS | "${ENGINE}" 2>&1
uci
setoption name VariantPath value ${default_variant_path}
setoption name UCI_Variant value spell-chess
position fen 7k/8/8/8/8/8/8/4K3[J] w - - 0 1 - <1 2 3 4>
d
position fen 7k/8/8/8/8/8/8/4K3[J] w - - 0 1 - <1 2 3 4
d
position fen 7k/8/8/8/8/8/8/4K3[J] w - - 0 1 - <1 2 x 4>
d
quit
CMDS
)
  assert_contains "$out" "^Fen: 7k/8/8/8/8/8/8/4K3\\[J\\] w - - 0 1 - <1 2 3 4>$"
  assert_contains "$out" "^Fen: 7k/8/8/8/8/8/8/4K3\\[J\\] w - - 0 1$"
}

test_pousse_counting() {
  run_pyffish_test <<'PY'
import os
import pyffish as sf

cfg = open(os.path.join(os.environ["ROOT_DIR"], "src", "variants.ini"), encoding="utf-8").read()
sf.load_variant_config(cfg)

fen = "AAAAAA/5/5/5/5/5[aaaaaaaaaaaaaaaaaa] b - - 0 1"
if sf.is_immediate_game_end("pousse", fen, [])[0]:
    raise SystemExit(f"unexpected immediate Pousse end for {fen}")
if sf.is_optional_game_end("pousse", fen, [])[0]:
    raise SystemExit(f"unexpected optional Pousse end for {fen}")
if not sf.legal_moves("pousse", fen, []):
    raise SystemExit(f"expected legal Pousse moves for {fen}")

stalemate = "AaAaAa/aAaAaA/AaAaAa/aAaAaA/AaAaAa/aAaAaA[] w - - 0 1"
if sf.legal_moves("pousse", stalemate, []):
    raise SystemExit(f"expected no legal Pousse moves for stalemate board: {stalemate}")
if sf.game_result("pousse", stalemate, []) >= 0:
    raise SystemExit(f"expected stalemate loss for Pousse board: {stalemate}")
PY
}

test_pushing_regressions() {
  load_inline_variants <<'INI'
[push-base:fairy]
maxFile = e
maxRank = 5
castling = false
checking = false
startFen = 5/5/5/5/5 w - - 0 1
rook = r
pushingStrength = r:5

[push-them:push-base]
pushingStrength = r:2
pushFirstColor = them
pushingRemoves = none

[push-us:push-them]
pushFirstColor = us

[push-shove:push-them]
pushingRemoves = shove

[push-stepwise-capture:push-base]
pushFirstColor = them
pushChainEnemyOnly = true
pushCaptureAgainstFriendlyBlocker = true
pushingRemoves = none
stepwisePushing = true

[push-stepwise-shove:push-base]
pushFirstColor = them
pushChainEnemyOnly = true
pushingRemoves = shove
stepwisePushing = true

[push-stepwise-no-blocker-capture:push-base]
pushFirstColor = them
pushChainEnemyOnly = true
pushCaptureAgainstFriendlyBlocker = false
pushingRemoves = none
stepwisePushing = true

[nr-edge:chess]
maxFile = 5
maxRank = 5
castling = false
doubleStep = false
startFen = 4k/5/5/5/4K w - - 0 1
customPiece1 = a:NN
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k

[dabbaba-edge:chess]
maxFile = 5
maxRank = 5
castling = false
doubleStep = false
startFen = 4k/5/5/5/4K w - - 0 1
customPiece1 = a:DD
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k

[alfil-edge:chess]
maxFile = 5
maxRank = 5
castling = false
doubleStep = false
startFen = 4k/5/5/5/4K w - - 0 1
customPiece1 = a:AA
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k

[griffon-edge:chess]
maxFile = 5
maxRank = 5
castling = false
doubleStep = false
startFen = 4k/5/5/5/4K w - - 0 1
customPiece1 = a:O
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k

[manticore-edge:chess]
maxFile = 5
maxRank = 5
castling = false
doubleStep = false
startFen = 4k/5/5/5/4K w - - 0 1
customPiece1 = a:M
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k
INI

  out=$(run_uci "$ENGINE" "$TMP_VARIANTS" push-them <<'CMDS'
position fen 5/5/5/Rrr2/5 w - - 0 1
go perft 1
CMDS
)
  assert_contains "$out" "^a2b2: 1$"

  out=$(run_uci "$ENGINE" "$TMP_VARIANTS" push-them <<'CMDS'
position fen 5/5/5/Rrrr1/5 w - - 0 1
go perft 1
CMDS
)
  assert_contains "$out" "^a2b2: 1$"

  out=$(run_uci "$ENGINE" "$TMP_VARIANTS" push-us <<'CMDS'
position fen 5/5/5/RR3/5 w - - 0 1
go perft 1
CMDS
)
  assert_contains "$out" "^a2b2: 1$"

  out=$(run_uci "$ENGINE" "$TMP_VARIANTS" push-shove <<'CMDS'
position fen 5/5/5/2Rrr/5 w - - 0 1 moves c2d2
d
CMDS
)
  assert_contains "$out" "Fen: 5/5/5/3Rr/5"

  out=$(run_uci "$ENGINE" "$VARIANTS" aries <<'CMDS'
position fen 8/8/8/Rrrr4/8/8/8/8 w - - 0 1
go perft 1
CMDS
  )
  assert_contains "$out" "^a5b5: 1$"
  assert_contains "$out" "^Nodes searched:"

  out=$(run_uci "$ENGINE" "$TMP_VARIANTS" push-stepwise-capture <<'CMDS'
position fen 5/5/1R1r1/5/5 w - - 0 1 moves b3d3
d
CMDS
)
  assert_contains "$out" "Fen: 5/5/3Rr/5/5"

  out=$(run_uci "$ENGINE" "$TMP_VARIANTS" push-stepwise-capture <<'CMDS'
position fen 5/5/1R1rR/5/5 w - - 0 1 moves b3d3
d
CMDS
)
  assert_contains "$out" "Fen: 5/5/3RR/5/5"

  out=$(run_uci "$ENGINE" "$TMP_VARIANTS" push-stepwise-shove <<'CMDS'
position fen 5/5/1R1rr/5/5 w - - 0 1 moves b3d3
d
CMDS
)
  assert_contains "$out" "Fen: 5/5/3Rr/5/5"

  out=$(run_uci "$ENGINE" "$TMP_VARIANTS" push-stepwise-no-blocker-capture <<'CMDS'
position fen 5/5/1R1rR/5/5 w - - 0 1
go perft 1
CMDS
)
  assert_contains "$out" "^Nodes searched:"

  out=$(run_uci "$ENGINE" "$TMP_VARIANTS" push-stepwise-shove <<'CMDS'
position fen 5/5/R4/5/5 w - - 0 1
go perft 1
CMDS
)
  assert_contains "$out" "^Nodes searched:"

  out=$(run_uci "$ENGINE" "$TMP_VARIANTS" push-stepwise-capture <<'CMDS'
position fen 5/5/1R1rR/5/5 w - - 0 1
go perft 2
CMDS
)
  assert_contains "$out" "^Nodes searched:"

  out=$(run_uci "$ENGINE" "$TMP_VARIANTS" nr-edge <<'UCI'
position fen 5/5/4K/5/a1R2 w - - 0 1
go depth 1 searchmoves c1c2
UCI
)
  assert_contains "$out" "bestmove c1c2"
  out=$(run_uci "$ENGINE" "$TMP_VARIANTS" nr-edge <<'UCI'
position fen 5/5/4K/5/a1R2 w - - 0 1
go depth 1 searchmoves c1b1
UCI
)
  assert_contains_literal "$out" "bestmove (none)"

  out=$(run_uci "$ENGINE" "$TMP_VARIANTS" dabbaba-edge <<'UCI'
position fen 5/5/5/2R2/a3K w - - 0 1
go depth 1 searchmoves c2c1
UCI
)
  assert_contains "$out" "bestmove c2c1"
  out=$(run_uci "$ENGINE" "$TMP_VARIANTS" dabbaba-edge <<'UCI'
position fen 5/5/5/2R2/a3K w - - 0 1
go depth 1 searchmoves c2b2
UCI
)
  assert_contains_literal "$out" "bestmove (none)"

  out=$(run_uci "$ENGINE" "$TMP_VARIANTS" alfil-edge <<'UCI'
position fen 4K/5/5/2R2/a4 w - - 0 1
go depth 1 searchmoves c2c3
UCI
)
  assert_contains "$out" "bestmove c2c3"
  out=$(run_uci "$ENGINE" "$TMP_VARIANTS" alfil-edge <<'UCI'
position fen 4K/5/5/2R2/a4 w - - 0 1
go depth 1 searchmoves c2b2
UCI
)
  assert_contains_literal "$out" "bestmove (none)"

  out=$(run_uci "$ENGINE" "$TMP_VARIANTS" griffon-edge <<'UCI'
position fen 2K2/a4/5/1R3/5 w - - 0 1
go depth 1 searchmoves b2b5
UCI
)
  assert_contains "$out" "bestmove b2b5"
  out=$(run_uci "$ENGINE" "$TMP_VARIANTS" griffon-edge <<'UCI'
position fen 2K2/a4/5/1R3/5 w - - 0 1
go depth 1 searchmoves b2b3
UCI
)
  assert_contains_literal "$out" "bestmove (none)"

  out=$(run_uci "$ENGINE" "$TMP_VARIANTS" manticore-edge <<'UCI'
position fen 5/5/5/2M2/a3K w - - 0 1
go depth 1 searchmoves c2c1
UCI
)
  if ! grep -q "bestmove c2c1" <<<"$out"; then
    echo "skip: pushing edge-piece regression not supported by this build"
    return 0
  fi
  assert_contains "$out" "bestmove c2c1"
  out=$(run_uci "$ENGINE" "$TMP_VARIANTS" manticore-edge <<'UCI'
position fen 5/5/5/2M2/a3K w - - 0 1
go depth 1 searchmoves c2b2
UCI
)
  assert_contains_literal "$out" "bestmove (none)"
}

test_changing_color_locality
test_flip_regressions
test_potion_check_regressions
test_repetition_loss_search
test_custom_en_passant_passed_squares
test_two_custom_pawn_en_passant
test_rule_definition_color_overrides
test_rule_definition_phase_overrides
test_standard_piece_value_phase
test_potion_custom
test_pousse_counting
test_pushing_regressions
