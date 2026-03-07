#!/bin/bash
# Incremental-key vs FEN-reload key consistency checks for selected variants.

set -euo pipefail

error() {
  echo "state-sync key test failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-./stockfish}
DEFAULT_VARIANT_PATH="variants.ini"
if [[ ! -f "${DEFAULT_VARIANT_PATH}" && -f "src/variants.ini" ]]; then
  DEFAULT_VARIANT_PATH="src/variants.ini"
fi

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

position_dump() {
  local variant_path="$1"
  local variant="$2"
  local pos_cmd="$3"
  cat <<CMDS | "$ENGINE"
uci
setoption name VariantPath value ${variant_path}
setoption name UCI_Variant value ${variant}
${pos_cmd}
d
quit
CMDS
}

assert_reload_key_match() {
  local variant_path="$1"
  local variant="$2"
  local pos_cmd="$3"

  local out fen key out2 key2
  out=$(position_dump "${variant_path}" "${variant}" "${pos_cmd}")
  if echo "$out" | grep -q "Unable to open file"; then
    echo "VariantPath load failed: ${variant_path}"
    echo "$out"
    return 1
  fi
  readarray -t parsed < <(extract_fen_and_key "$out")
  fen="${parsed[0]}"
  key="${parsed[1]}"

  out2=$(position_dump "${variant_path}" "${variant}" "position fen ${fen}")
  key2=$(echo "$out2" | sed -n 's/^Key: //p' | tail -n1)

  if [[ "${key}" != "${key2}" ]]; then
    echo "Key mismatch for ${variant}"
    echo "position: ${pos_cmd}"
    echo "fen: ${fen}"
    echo "key: ${key}"
    echo "reload key: ${key2}"
    return 1
  fi
}

echo "state-sync key tests started"

# 1) Seirawan gating consumes a hand piece; key must match after FEN reload.
assert_reload_key_match "${DEFAULT_VARIANT_PATH}" "seirawan" "position startpos moves b1a3h a7a6"

# 2) Prison capture updates reserve state; key must match after FEN reload.
tmp_ini=$(mktemp)
cat > "$tmp_ini" <<'INI'
[prsync:chess]
pieceDrops = true
captureType = prison
castling = false

[exsync:chess]
pieceDrops = true
captureType = prison
hostageExchange = p:p
castling = false
startFen = 8/8/8/3p1p2/2P1P3/8/8/4K2k w - - 0 1
INI
assert_reload_key_match "$tmp_ini" "prsync" "position startpos moves e2e4 d7d5 e4d5"

# 3) Prison exchange drops mutate both prison/hand counts; key must match after FEN reload.
assert_reload_key_match "$tmp_ini" "exsync" "position startpos moves c4d5 f5e4 P#P@a2"
rm -f "$tmp_ini"

# 4) Flip-enclosed games: color-flip captures must keep incremental key in sync.
assert_reload_key_match "${DEFAULT_VARIANT_PATH}" "ataxx" "position startpos moves g1f2"
assert_reload_key_match "${DEFAULT_VARIANT_PATH}" "flipello" "position startpos moves P@e3"

echo "state-sync key tests OK"
