#!/bin/bash

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "potion undo parity test"

VARIANT_PATH="${VARIANTS}"

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

echo "potion undo parity tests started"

# Spell-chess: a single potion+move round-trip through undo must restore the start state.
assert_xboard_matches_uci "${VARIANT_PATH}" "spell-chess" \
  $'usermove f@a6,e2e4\nundo' \
  "position startpos"

# Spell-chess: undoing only the second potion move must match the one-ply prefix state.
assert_xboard_matches_uci "${VARIANT_PATH}" "spell-chess" \
  $'usermove f@a6,e2e4\nusermove j@a7,a8a2\nundo' \
  "position startpos moves f@a6,e2e4"

# Spell-chess: nested undo over both potion plies must restore the exact start key.
assert_xboard_matches_uci "${VARIANT_PATH}" "spell-chess" \
  $'usermove f@a6,e2e4\nusermove j@a7,a8a2\nundo\nundo' \
  "position startpos"

load_inline_variants <<'INI'
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
tmp_ini="${FSX_TMP_INI}"

# Mixed potion + prison + exchange path: undoing the exchange must match the pre-exchange prefix.
assert_xboard_matches_uci "${tmp_ini}" "spellprisonex" \
  $'usermove q@c6,c4d5\nusermove f5e4\nusermove P#P@a2\nundo' \
  "position startpos moves q@c6,c4d5 f5e4"

# Mixed path: undoing all plies must restore the custom start state exactly.
assert_xboard_matches_uci "${tmp_ini}" "spellprisonex" \
  $'usermove q@c6,c4d5\nusermove f5e4\nusermove P#P@a2\nundo\nundo\nundo' \
  "position startpos"

echo "potion undo parity tests passed"
