#!/bin/bash

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
export ROOT_DIR
source "${ROOT_DIR}/tests/lib/uci.sh"
ENGINE=${1:-src/stockfish}
PYTHON=${PYTHON:-python3}
VARIANT_PATH=${VARIANT_PATH:-src/variants.ini}
VARIANTS="${VARIANTS:-${VARIANT_PATH}}"
export VARIANTS
INCOMPLETE_VARIANT_PATH=${INCOMPLETE_VARIANT_PATH:-src/variants-incomplete.ini}
VLB_ENGINE=${VLB_ENGINE:-src/stockfish-vlb}
LARGE_ENGINE=${LARGE_ENGINE:-src/stockfish-large}

rm -rf "${ROOT_DIR}/.local/build"
ENGINE_RUN_DIR="${ROOT_DIR}/.local/build/local-regression-engine"
mkdir -p "${ENGINE_RUN_DIR}"
DEFAULT_ENGINE_COPY="${ENGINE_RUN_DIR}/stockfish"
cp -f "${ENGINE}" "${DEFAULT_ENGINE_COPY}"
chmod +x "${DEFAULT_ENGINE_COPY}"
ENGINE="${DEFAULT_ENGINE_COPY}"
export ENGINE

run_step() {
  local label="$1"
  shift
  echo "== ${label} =="
  TIMEFORMAT='elapsed %3R s'
  time "$@"
}

cleanup_local_variants() {
  if [[ -n "${FSX_TMP_INI:-}" && -e "${FSX_TMP_INI}" ]]; then
    rm -f "${FSX_TMP_INI}"
  fi
  FSX_TMP_INI=
  TMP_VARIANTS=
}

prepare_local_inline_variants() {
  FSX_TMP_INI=$(mktemp "${TMPDIR:-/tmp}/fsx-local-variants-XXXXXX.ini")
  export FSX_TMP_INI
  TMP_VARIANTS="${FSX_TMP_INI}"
  export TMP_VARIANTS
  cat >"${FSX_TMP_INI}" <<'INI'
[spellprisonex:chess]
potions = true
freezePotion = q
jumpPotion = r
potionCooldown = 3
pieceDrops = true
captureType = prison
hostageExchange = p:p
castling = false
startFen = 8/8/8/3p1p2/2P1P3/8/8/4K2k[Qq] w - - 0 1
INI
}

ensure_local_inline_variants() {
  if [[ -z "${FSX_TMP_INI:-}" || ! -f "${FSX_TMP_INI}" ]]; then
    prepare_local_inline_variants
  fi
}

extract_fen_and_key() {
  local output="$1"
  local fen key
  output=$(printf '%s' "$output" | tr -d '\r')
  fen=$(echo "$output" | sed -n 's/^Fen: //p' | tail -n1)
  key=$(echo "$output" | sed -n 's/^Key: //p' | tail -n1)
  if [[ -z "${fen}" || -z "${key}" ]]; then
    return 1
  fi
  echo "${fen}"$'\n'"${key}"
}

uci_dump() {
  local variant_path="$1"
  local variant="$2"
  local pos_cmd="$3"

  run_uci "$ENGINE" "${variant_path}" "${variant}" <<CMDS
${pos_cmd}
d
CMDS
}

xboard_dump() {
  local variant_path="$1"
  local variant="$2"
  local cmds="$3"

  run_expect "$ENGINE" <<EOF
$(expect_engine_setup xboard)
   send "protover 2\n"
   send "option VariantPath=${variant_path}\n"
   send "variant ${variant}\n"
   send "new\n"
   send "force\n"
$(while IFS= read -r line; do
  [[ -z "${line}" ]] && continue
  printf '   send "%s\\n"\n' "${line}"
done <<< "${cmds}")
   send "d\n"
   send "quit\n"
   expect eof
EOF
}

assert_xboard_matches_uci() {
  local variant_path="$1"
  local variant="$2"
  local xboard_cmds="$3"
  local pos_cmd="$4"

  local xb_out ref_out xb_fen xb_key ref_fen ref_key
  xb_out=$(xboard_dump "${variant_path}" "${variant}" "${xboard_cmds}")
  ref_out=$(uci_dump "${variant_path}" "${variant}" "${pos_cmd}")

  readarray -t xb_parsed < <(extract_fen_and_key "${xb_out}")
  readarray -t ref_parsed < <(extract_fen_and_key "${ref_out}")
  xb_fen="${xb_parsed[0]}"
  xb_key="${xb_parsed[1]}"
  ref_fen="${ref_parsed[0]}"
  ref_key="${ref_parsed[1]}"

  if [[ "${xb_fen}" != "${ref_fen}" || "${xb_key}" != "${ref_key}" ]]; then
    echo "XBoard/UCI mismatch for ${variant}"
    echo "xboard commands:"
    printf '%s\n' "${xboard_cmds}"
    echo "uci position: ${pos_cmd}"
    echo "xboard fen: ${xb_fen}"
    echo "uci fen: ${ref_fen}"
    echo "xboard key: ${xb_key}"
    echo "uci key: ${ref_key}"
    return 1
  fi
}

test_bombardment() {
  if ! variant_available "$ENGINE" bombardment "$VARIANT_PATH"; then
    echo "bombardment variant not available in this build; skipping bombardment test"
    return 0
  fi

  local out
  out=$(run_uci "$ENGINE" "$VARIANT_PATH" bombardment <<'EOF'
position startpos
go perft 1
EOF
)
  assert_contains "$out" "^a2a3: 1$"
  assert_contains "$out" "^a2b3: 1$"
  assert_contains "$out" "^a2a2x: 1$"
  assert_not_contains "$out" "^a2b2:"

  out=$(run_uci "$ENGINE" "$VARIANT_PATH" bombardment <<'EOF'
position startpos
go movetime 500
EOF
)
  assert_contains "$out" "^bestmove "
  assert_not_contains "$out" "score mate"

  out=$(run_uci "$ENGINE" "$VARIANT_PATH" bombardment <<'EOF'
position startpos moves a2a3
d
EOF
)
  assert_contains "$out" "Fen: mmmmmmmm/mmmmmmmm/8/8/8/M7/1MMMMMMM/MMMMMMMM b - - 1 1"

  out=$(run_uci "$ENGINE" "$VARIANT_PATH" bombardment <<'EOF'
position fen 8/8/2mmm3/2mMm3/2mmm3/8/8/M7 w - - 0 1 moves d5d5x
d
EOF
)
  assert_contains "$out" "Fen: 8/8/8/8/8/8/8/M7"
}

test_cowboys_opening() {
  if ! variant_available "$ENGINE" cowboys "$VARIANTS"; then
    echo "cowboys variant not available in this build; skipping cowboys opening test"
    return 0
  fi

  local out
  out=$(run_uci "$ENGINE" "$VARIANTS" cowboys <<'EOF'
position startpos
go depth 7
EOF
)
  assert_contains "$out" "^bestmove "
}

test_slide5_regression() {
  local perft_out tty_out
  perft_out=$(cat <<EOF | "${ENGINE}"
uci
setoption name Hash value 1
setoption name Clear Hash
setoption name VariantPath value ${VARIANT_PATH}
setoption name UCI_Variant value slide-5
position startpos
go perft 1
position startpos moves A@a1,b1
go perft 1
quit
EOF
)
  assert_contains_literal "${perft_out}" "Nodes searched: 10"
  assert_contains_literal "${perft_out}" "Nodes searched: 9"

  tty_out=$(run_expect "$ENGINE" <<EOF
log_user 1
set timeout 10
spawn ${ENGINE}
expect "by Fabian Fichter"
send "setoption name Hash value 1\r"
send "setoption name Clear Hash\r"
send "setoption name VariantPath value ${VARIANT_PATH}\r"
send "setoption name UCI_Variant value slide-5\r"
send "position startpos\r"
send "go movetime 10\r"
expect -re "bestmove .*"
send "position startpos moves A@a1,b1\r"
send "go movetime 10\r"
expect -re "bestmove .*"
send "quit\r"
expect eof
EOF
)
  assert_contains "$tty_out" "^info depth "
  assert_contains "$tty_out" "^bestmove "
  assert_not_contains "$tty_out" "score mate"
  assert_not_contains "$tty_out" "^bestmove A@e5,e4$"
}

test_pond() {
  local output

  output=$(run_uci "$ENGINE" "$VARIANTS" pond <<'CMDS'
position fen 1tt1/T1et/2TF/T1ef[EEEEEEeeeee] w - - 0 11 {3 2}
go movetime 1000
CMDS
)
  assert_contains "$output" "^bestmove "
  assert_not_contains "$output" "(Assertion|Segmentation fault|Aborted|Illegal instruction)"

  output=$(run_uci "$ENGINE" "$VARIANTS" pond <<'CMDS'
position startpos moves E@b3 E@d3 E@c3 E@c2
d
CMDS
)
  assert_contains_literal "$output" "Fen: 4/4/2e1/4[EEEEEEEEEEEeeeeeeeeeee] w - - 0 3 {2 1}"

  output=$(run_uci "$ENGINE" "$VARIANTS" pond <<'CMDS'
position fen 4/4/1Tt1/4[] w - - 0 1 {0 0}
go perft 1
CMDS
)
  assert_contains_literal "$output" "b2b1: 1"
  assert_contains_literal "$output" "b2a2: 1"
  assert_contains_literal "$output" "b2b3: 1"
  assert_not_contains_literal "$output" "b2c2:"

  output=$(run_uci "$ENGINE" "$VARIANTS" pond <<'CMDS'
position fen 4/4/Ft2/4[] w - - 0 1 {0 0}
go perft 1
CMDS
)
  assert_contains_literal "$output" "a2a1: 1"
  assert_contains_literal "$output" "a2a3: 1"
  assert_contains_literal "$output" "a2c2: 1"

  output=$(run_uci "$ENGINE" "$VARIANTS" pond <<'CMDS'
position fen 4/4/1F2/4[] w - - 0 1 {0 0}
go perft 1
CMDS
)
  assert_contains_literal "$output" "b2d2: 1"

  output=$(run_uci "$ENGINE" "$VARIANTS" pond <<'CMDS'
position fen 4/4/1Ft1/4[] w - - 0 1 {0 0}
go perft 1
CMDS
)
  assert_contains_literal "$output" "b2d2: 1"

  output=$(run_uci "$ENGINE" "$VARIANTS" pond <<'CMDS'
position fen 4/4/1FtT/4[] w - - 0 1 {0 0}
go perft 1
CMDS
)
  assert_not_contains_literal "$output" "b2d2:"

  output=$(run_uci "$ENGINE" "$VARIANTS" pond <<'CMDS'
position fen TTT1/4/3T/TTT1 w - - 0 1 {0 0} moves d2d3
d
CMDS
)
  assert_contains_literal "$output" "Fen: 4/3T/4/4[] b - - 1 1 {6 0}"

  output=$(run_uci "$ENGINE" "$VARIANTS" pond <<'CMDS'
position fen T3/2T1/1T2/T3 w - - 0 1 {0 0} moves a4b4
d
CMDS
)
  assert_contains_literal "$output" "Fen: 1T2/4/4/4[] b - - 1 1 {3 0}"

  output=$(run_uci "$ENGINE" "$VARIANTS" pond <<'CMDS'
position fen 1T2/4/4/4[] b - - 1 1 {3 0}
go depth 2
CMDS
)
  assert_contains_literal "$output" "bestmove (none)"
}

test_potion_undo_parity() {
  local variant_path="${VARIANTS}"
  local xboard_fen uci_fen
  ensure_local_inline_variants

  assert_xboard_matches_uci "${variant_path}" "spell-chess" \
    $'usermove f@a6,e2e4\nundo' \
    "position startpos"

  assert_xboard_matches_uci "${variant_path}" "spell-chess" \
    $'usermove f@a6,e2e4\nusermove j@a7,a8a2\nundo' \
    "position startpos moves f@a6,e2e4"

  assert_xboard_matches_uci "${variant_path}" "spell-chess" \
    $'usermove f@a6,e2e4\nusermove j@a7,a8a2\nundo\nundo' \
    "position startpos"
  assert_xboard_matches_uci "${TMP_VARIANTS}" "spellprisonex" \
    $'usermove q@c6,c4d5\nusermove f5e4\nusermove P#P@a2\nundo' \
    "position startpos moves q@c6,c4d5 f5e4"

  assert_xboard_matches_uci "${TMP_VARIANTS}" "spellprisonex" \
    $'usermove q@c6,c4d5\nusermove f5e4\nusermove P#P@a2\nundo\nundo\nundo' \
    "position startpos"
}

VLB_CAPABLE_ENGINE="${ENGINE}"
if [[ -x "${VLB_ENGINE}" ]]; then
  VLB_CAPABLE_ENGINE="${VLB_ENGINE}"
fi

cd "${ROOT_DIR}"
prepare_local_inline_variants

run_step "bombardment" test_bombardment
run_step "cowboys opening" test_cowboys_opening
run_step "slide5 regression" test_slide5_regression
run_step "pond" test_pond
run_step "potion undo parity" test_potion_undo_parity

run_step "fast regression" timeout 5m bash tests/fast-regression.sh "${ENGINE}"
run_step "invalid scalar regression" timeout 30s bash tests/invalid-scalar-regression.sh "${ENGINE}"
run_step "movegen regressions" timeout 90s bash tests/movegen-regressions.sh "${ENGINE}"
run_step "royal variant regressions" timeout 2m bash tests/royal-variant-regressions.sh "${ENGINE}" "${VARIANT_PATH}"
run_step "drop regressions" timeout 2m bash tests/drop-regressions.sh "${ENGINE}"
run_step "fairy notation regressions" timeout 2m bash tests/fairy-notation-regressions.sh "${ENGINE}"
run_step "wrapping topology" timeout 90s bash tests/wrapping-topology.sh "${ENGINE}"
run_step "unorthodox interactions" timeout 90s bash tests/unorthodox-interactions.sh "${ENGINE}"
run_step "universal hopper" timeout 90s bash tests/universal-hopper.sh "${ENGINE}"
run_step "all-vars regression" timeout 60m bash tests/allvars-regression.sh
if [[ -x "${LARGE_ENGINE}" ]]; then
  ENGINE="${LARGE_ENGINE}"
  export ENGINE
fi
run_step "protocol" timeout 2m bash tests/protocol.sh "${ENGINE}"
run_step "bench stdin" timeout 60s bash tests/bench-stdin.sh "${ENGINE}"
run_step "ponder stop" timeout 2m bash tests/ponder-stop.sh "${ENGINE}"
run_step "xboard regressions" timeout 2m bash tests/xboard-regressions.sh "${ENGINE}"
run_step "battleotk" timeout 2m bash tests/battleotk.sh "${ENGINE}" "${VARIANT_PATH}"
run_step "gating regressions" timeout 60s bash tests/gating-regressions.sh "${ENGINE}"
run_step "in-place transform undo" timeout 60s bash tests/in-place-transform-undo.sh "${ENGINE}"
run_step "bycatch undo parity" timeout 60s bash tests/bycatch-undo-parity.sh "${ENGINE}"
run_step "StateInfo regressions" timeout 3m bash tests/stateinfo-regressions.sh "${ENGINE}"
run_step "verbosity" timeout 60s bash tests/verbosity.sh "${ENGINE}"
run_step "state sync key" timeout 5m bash tests/state-sync-key.sh "${ENGINE}"
run_step "new variants smoke" timeout 30m bash tests/new-variants-smoke.sh "${ENGINE}" "${VARIANT_PATH}"
run_step "setup chess" timeout 2m bash tests/setup-chess.sh "${ENGINE}"
run_step "stationary castling" timeout 60s bash tests/stationary-castling.sh "${ENGINE}"
run_step "move morph" timeout 60s bash tests/move-morph.sh "${ENGINE}" "${VARIANT_PATH}"
run_step "petrify transfer" timeout 60s bash tests/petrify-transfer.sh "${ENGINE}"
run_step "dots and boxes" timeout 5m bash tests/dots-and-boxes.sh "${ENGINE}" "${VARIANT_PATH}" "${INCOMPLETE_VARIANT_PATH}" "${LARGE_ENGINE}" "${VLB_ENGINE}"
run_step "seega" timeout 60s bash tests/seega.sh "${ENGINE}" "${VARIANT_PATH}"
run_step "rose" timeout 60s bash tests/rose.sh "${ENGINE}"
run_step "bent riders" timeout 60s bash tests/bent-riders.sh "${ENGINE}" "${VARIANT_PATH}"
run_step "bent rider evasions" timeout 60s bash tests/bent-rider-evasion.sh "${ENGINE}" "${VARIANT_PATH}"
run_step "hex boards" timeout 60s bash tests/test_hex_boards.sh "${VLB_CAPABLE_ENGINE}" "${VARIANT_PATH}"
run_step "connect region 3" timeout 60s bash tests/connect-region3.sh "${ENGINE}" "${VARIANT_PATH}"
run_step "kopano" timeout 60s bash tests/kopano.sh "${ENGINE}" "${VARIANT_PATH}"
run_step "konobi" timeout 60s bash tests/konobi.sh "${ENGINE}" "${VARIANT_PATH}"
run_step "whaleshogi" timeout 60s bash tests/whaleshogi.sh "${ENGINE}"
run_step "dead pieces" timeout 60s bash tests/dead-pieces.sh "${ENGINE}"
run_step "stationary capture" timeout 60s bash tests/stationary-capture.sh "${ENGINE}"
run_step "spell freeze regressions" timeout 60s bash tests/spell-freeze-regressions.sh "${ENGINE}"
run_step "spell potion movegen" timeout 60s bash tests/spell-potion-movegen.sh "${ENGINE}"
run_step "asym rider checkers" timeout 60s bash tests/asym-rider-checkers.sh "${ENGINE}"
run_step "alfil dabbaba riders" timeout 2m bash tests/alfil-dabbaba-riders.sh "${ENGINE}"
run_step "concurrent variant magics" timeout 60s bash tests/concurrent-variant-magics.sh "${ENGINE}"
run_step "NNUE variant dimension guard" timeout 60s bash tests/nnue-variant-dimension-guard.sh "${ENGINE}"
run_step "NNUE affine regression" timeout 2m bash tests/nnue-affine-regression.sh
run_step "NNUE export failure" timeout 60s bash tests/nnue-export-failure.sh "${ENGINE}"
run_step "rootmove searchmoves" timeout 60s bash tests/rootmove-searchmoves.sh "${ENGINE}"
run_step "jump capture effects" timeout 60s bash tests/jump-capture-effects.sh "${ENGINE}"
run_step "edge insert" timeout 60s bash tests/edge-insert.sh "${ENGINE}"
run_step "extinction" timeout 60s bash tests/test_extinction.sh "${ENGINE}"
run_step "promotion consume in hand" timeout 60s bash tests/promotion-consume-in-hand.sh "${ENGINE}"
run_step "promotion require in hand" timeout 60s bash tests/promotion-require-in-hand.sh "${ENGINE}"
run_step "kings or lemmings" timeout 60s bash tests/kings-or-lemmings.sh "${ENGINE}" "${VARIANT_PATH}"
run_step "hindustani" timeout 60s bash tests/hindustani.sh "${ENGINE}" "${VARIANT_PATH}"
run_step "sacrifice" timeout 60s bash tests/sacrifice.sh "${ENGINE}" "${VARIANT_PATH}"
run_step "pulling" timeout 60s bash tests/pulling.sh "${ENGINE}" "${VARIANT_PATH}"
run_step "swapping" timeout 60s bash tests/swapping.sh "${ENGINE}" "${VARIANT_PATH}"
run_step "must drop by color" timeout 60s bash tests/must-drop-by-color.sh "${ENGINE}"
run_step "must capture by color" timeout 60s bash tests/must-capture-by-color.sh "${ENGINE}"
run_step "self capture color" timeout 60s bash tests/self-capture-color.sh "${ENGINE}"
run_step "self capture types" timeout 60s bash tests/self-capture-types.sh "${ENGINE}"
run_step "largeboard seirawan" timeout 60s bash tests/largeboard-seirawan.sh
run_step "VLB gale smoke" timeout 60s bash tests/vlb-gale-smoke.sh "${VLB_CAPABLE_ENGINE}" "${VARIANT_PATH}"
run_step "VLB lame riders" timeout 60s bash tests/vlb-lame-riders.sh "${VLB_CAPABLE_ENGINE}"
run_step "VLB symbol check" timeout 60s bash tests/vlb-symbol-check.sh "${VLB_CAPABLE_ENGINE}"
run_step "VLB symbol fen" timeout 60s bash tests/vlb-symbol-fen.sh "${VLB_CAPABLE_ENGINE}"
run_step "VLB symbol options" timeout 60s bash tests/vlb-symbol-options.sh "${VLB_CAPABLE_ENGINE}"
run_step "VLB symbol san" timeout 60s "${PYTHON}" tests/vlb-symbol-san.py
run_step "variant perft" timeout 30m bash tests/perft.sh all "${ENGINE}"

cleanup_local_variants

echo "local regression suite passed"
