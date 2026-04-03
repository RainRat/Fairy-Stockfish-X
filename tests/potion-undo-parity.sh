#!/bin/bash

set -euo pipefail

error() {
  echo "potion undo parity test failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
ENGINE="${1:-${REPO_ROOT}/src/stockfish}"
DEFAULT_VARIANT_PATH="${REPO_ROOT}/src/variants.ini"
if [[ ! -f "${DEFAULT_VARIANT_PATH}" && -f "src/${REPO_ROOT}/src/variants.ini" ]]; then
  DEFAULT_VARIANT_PATH="src/${REPO_ROOT}/src/variants.ini"
fi
VARIANT_PATH=${2:-${DEFAULT_VARIANT_PATH}}

extract_fen_and_key() {
  local output="$1"
  local fen key
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
  local variant_path_cmd=""
  if [[ -n "${variant_path}" ]]; then
    variant_path_cmd="setoption name VariantPath value ${variant_path}"
  fi
  cat <<CMDS | "${ENGINE}"
uci
${variant_path_cmd}
setoption name UCI_Variant value ${variant}
${pos_cmd}
d
quit
CMDS
}

xboard_dump() {
  local variant_path="$1"
  local variant="$2"
  local cmds="$3"
  cat <<CMDS | "${ENGINE}"
xboard
protover 2
option VariantPath=${variant_path}
variant ${variant}
new
force
${cmds}
d
quit
CMDS
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

tmp_ini=$(mktemp)
trap 'rm -f "${tmp_ini}"' EXIT

cat > "${tmp_ini}" <<'INI'
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

# Mixed potion + prison + exchange path: undoing the exchange must match the pre-exchange prefix.
assert_xboard_matches_uci "${tmp_ini}" "spellprisonex" \
  $'usermove q@c6,c4d5\nusermove f5e4\nusermove P#P@a2\nundo' \
  "position startpos moves q@c6,c4d5 f5e4"

# Mixed path: undoing all plies must restore the custom start state exactly.
assert_xboard_matches_uci "${tmp_ini}" "spellprisonex" \
  $'usermove q@c6,c4d5\nusermove f5e4\nusermove P#P@a2\nundo\nundo\nundo' \
  "position startpos"

echo "potion undo parity tests passed"