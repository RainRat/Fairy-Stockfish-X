#!/bin/bash

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
export ROOT_DIR
source "${ROOT_DIR}/tests/lib/uci.sh"
setup_test_context "${1:-}" "${2:-}" "local regression inline"

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
  fsx_add_exit_cleanup cleanup_tmp_ini
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
position startpos moves a1a1x
d
EOF
)
  assert_contains "$out" "Fen: mmmmmmmm/mmmmmmmm/8/8/8/8/2MMMMMM/2MMMMMM b - - 0 1"

  out=$(run_uci "$ENGINE" "$VARIANT_PATH" bombardment <<'EOF'
position fen 8/8/2mmm3/2mMm3/2mmm3/8/8/M7 w - - 0 1 moves d5d5x
d
EOF
)
  assert_contains "$out" "Fen: 8/8/8/8/8/8/8/M7 b - - 0 1"
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

test_bombardment
test_cowboys_opening
test_slide5_regression
test_pond
test_potion_undo_parity
