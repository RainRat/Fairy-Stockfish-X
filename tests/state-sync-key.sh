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

extract_perft1_nodes() {
  local output="$1"
  echo "$output" | sed -n 's/^Nodes searched: //p' | tail -n1
}

extract_perft1_moves() {
  local output="$1"
  echo "$output" | awk -F: '/^[a-zA-Z0-9@#,+=-]+: /{print $1}' | sort
}

extract_final_eval() {
  local output="$1"
  echo "$output" | sed -n 's/^Final evaluation[[:space:]]*//p' | tail -n1
}

position_dump() {
  local variant_path="$1"
  local variant="$2"
  local pos_cmd="$3"
  local variant_path_cmd=""
  if [[ -n "${variant_path}" ]]; then
    variant_path_cmd="setoption name VariantPath value ${variant_path}"
  fi
  cat <<CMDS | "$ENGINE"
uci
${variant_path_cmd}
setoption name UCI_Variant value ${variant}
${pos_cmd}
d
quit
CMDS
}

bestmove_for_position() {
  local variant_path="$1"
  local variant="$2"
  local pos_cmd="$3"
  local variant_path_cmd=""
  if [[ -n "${variant_path}" ]]; then
    variant_path_cmd="setoption name VariantPath value ${variant_path}"
  fi
  cat <<CMDS | "$ENGINE" | sed -n 's/^bestmove //p' | awk '{print $1}' | tail -n1
uci
${variant_path_cmd}
setoption name UCI_Variant value ${variant}
setoption name Threads value 1
${pos_cmd}
go depth 1
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

assert_reload_perft1_match() {
  local variant_path="$1"
  local variant="$2"
  local pos_cmd="$3"

  local out fen out_reload nodes nodes_reload
  out=$(position_dump "${variant_path}" "${variant}" "${pos_cmd}
go perft 1")
  readarray -t parsed < <(extract_fen_and_key "$out")
  fen="${parsed[0]}"
  nodes=$(extract_perft1_nodes "$out")

  out_reload=$(position_dump "${variant_path}" "${variant}" "position fen ${fen}
go perft 1")
  nodes_reload=$(extract_perft1_nodes "$out_reload")

  if [[ -z "${nodes}" || -z "${nodes_reload}" || "${nodes}" != "${nodes_reload}" ]]; then
      echo "Perft1 mismatch for ${variant}"
      echo "position: ${pos_cmd}"
      echo "fen: ${fen}"
      echo "nodes: ${nodes}"
      echo "reload nodes: ${nodes_reload}"
      return 1
  fi
}

assert_reload_perft1_moves_match() {
  local variant_path="$1"
  local variant="$2"
  local pos_cmd="$3"

  local out fen out_reload
  local moves moves_reload
  out=$(position_dump "${variant_path}" "${variant}" "${pos_cmd}
go perft 1")
  readarray -t parsed < <(extract_fen_and_key "$out")
  fen="${parsed[0]}"
  moves=$(extract_perft1_moves "$out")

  out_reload=$(position_dump "${variant_path}" "${variant}" "position fen ${fen}
go perft 1")
  moves_reload=$(extract_perft1_moves "$out_reload")

  if [[ -z "${moves}" || -z "${moves_reload}" || "${moves}" != "${moves_reload}" ]]; then
      echo "Perft1 move-set mismatch for ${variant}"
      echo "position: ${pos_cmd}"
      echo "fen: ${fen}"
      echo "moves:"
      printf '%s\n' "${moves}"
      echo "reload moves:"
      printf '%s\n' "${moves_reload}"
      return 1
  fi
}

assert_reload_eval_match() {
  local variant_path="$1"
  local variant="$2"
  local pos_cmd="$3"

  local out fen out_reload eval eval_reload
  out=$(position_dump "${variant_path}" "${variant}" "${pos_cmd}
eval")
  readarray -t parsed < <(extract_fen_and_key "$out")
  fen="${parsed[0]}"
  eval=$(extract_final_eval "$out")

  out_reload=$(position_dump "${variant_path}" "${variant}" "position fen ${fen}
eval")
  eval_reload=$(extract_final_eval "$out_reload")

  if [[ -z "${eval}" || -z "${eval_reload}" || "${eval}" != "${eval_reload}" ]]; then
      echo "Eval mismatch for ${variant}"
      echo "position: ${pos_cmd}"
      echo "fen: ${fen}"
      echo "eval: ${eval}"
      echo "reload eval: ${eval_reload}"
      return 1
  fi
}

assert_distinct_position_keys() {
  local variant_path="$1"
  local variant="$2"
  local fen_a="$3"
  local fen_b="$4"

  local out_a out_b key_a key_b
  out_a=$(position_dump "${variant_path}" "${variant}" "position fen ${fen_a}")
  out_b=$(position_dump "${variant_path}" "${variant}" "position fen ${fen_b}")
  key_a=$(echo "$out_a" | sed -n 's/^Key: //p' | tail -n1)
  key_b=$(echo "$out_b" | sed -n 's/^Key: //p' | tail -n1)

  if [[ -z "${key_a}" || -z "${key_b}" || "${key_a}" == "${key_b}" ]]; then
    echo "Distinct-position key collision for ${variant}"
    echo "fen A: ${fen_a}"
    echo "key A: ${key_a}"
    echo "fen B: ${fen_b}"
    echo "key B: ${key_b}"
    return 1
  fi
}

assert_progressive_reload_keys() {
  local variant_path="$1"
  local variant="$2"
  local base_pos_cmd="$3"
  local plies="$4"

  local moves=""
  local pos_cmd="${base_pos_cmd}"
  for ((i=1; i<=plies; i++)); do
      assert_reload_key_match "${variant_path}" "${variant}" "${pos_cmd}"

      local bm
      bm=$(bestmove_for_position "${variant_path}" "${variant}" "${pos_cmd}")
      if [[ -z "${bm}" || "${bm}" == "(none)" ]]; then
          break
      fi

      if [[ -z "${moves}" ]]; then
          moves="${bm}"
      else
          moves="${moves} ${bm}"
      fi
      pos_cmd="${base_pos_cmd} moves ${moves}"
  done
}

echo "state-sync key tests started"

# 1) Seirawan gating consumes a hand piece; key must match after FEN reload.
assert_reload_key_match "" "seirawan" "position startpos moves b1a3h a7a6"
assert_progressive_reload_keys "" "seirawan" "position startpos" 8
assert_reload_perft1_match "" "seirawan" "position startpos moves b1a3h a7a6"

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

[commitkeys:chess]
commitGates = true
castling = false
startFen = 4q3/4k3/8/8/8/8/8/8/4K3/4Q3 w - - 0 1

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
assert_reload_key_match "$tmp_ini" "prsync" "position startpos moves e2e4 d7d5 e4d5"

# 3) Prison exchange drops mutate both prison/hand counts; key must match after FEN reload.
assert_reload_key_match "$tmp_ini" "exsync" "position startpos moves c4d5 f5e4 P#P@a2"
assert_progressive_reload_keys "$tmp_ini" "exsync" "position startpos" 10
assert_reload_perft1_match "$tmp_ini" "exsync" "position startpos moves c4d5 f5e4 P#P@a2"

# 4) Commit-gates drops should preserve key consistency through FEN reload.
assert_reload_key_match "$tmp_ini" "commitkeys" "position startpos moves e1d1 e8d8"
assert_progressive_reload_keys "$tmp_ini" "commitkeys" "position startpos" 8
assert_reload_perft1_match "$tmp_ini" "commitkeys" "position startpos moves e1d1 e8d8"
# 4b) Commit-gate reserve rows must affect key identity even when board occupancy is identical.
assert_distinct_position_keys "$tmp_ini" "commitkeys" \
  "4q3/4k3/8/8/8/8/8/8/4K3/4Q3 w - - 0 1" \
  "8/4k3/8/8/8/8/8/8/4K3/8 w - - 0 1"

# 4c) Potion + prison + exchange transitions must keep incremental state in sync.
assert_reload_key_match "$tmp_ini" "spellprisonex" "position startpos moves q@c6,c4d5"
assert_reload_key_match "$tmp_ini" "spellprisonex" "position startpos moves q@c6,c4d5 f5e4 P#P@a2"
assert_reload_perft1_match "$tmp_ini" "spellprisonex" "position startpos moves q@c6,c4d5 f5e4 P#P@a2"
assert_reload_perft1_moves_match "$tmp_ini" "spellprisonex" "position startpos moves q@c6,c4d5"
assert_reload_perft1_moves_match "$tmp_ini" "spellprisonex" "position startpos moves q@c6,c4d5 f5e4 P#P@a2"
assert_reload_eval_match "$tmp_ini" "spellprisonex" "position startpos moves q@c6,c4d5 f5e4 P#P@a2"
assert_distinct_position_keys "$tmp_ini" "spellprisonex" \
  "8/8/8/3P1p2/4P3/8/8/4K2k[q#p] b - - 0 1" \
  "8/8/8/3P1p2/4P3/8/8/4K2k[q#p] b - - 0 1 f:c6 <0 0 0 0>"
rm -f "$tmp_ini"

# 5) Flip-enclosed games: color-flip captures must keep incremental key in sync.
assert_reload_key_match "" "ataxx" "position startpos moves g1f2"
assert_reload_key_match "" "flipello" "position startpos moves P@e3"
assert_reload_perft1_match "" "ataxx" "position startpos moves g1f2"
assert_reload_eval_match "" "ataxx" "position startpos moves g1f2"
assert_reload_eval_match "" "flipello" "position startpos moves P@e3"

# 6) Spell-chess potion state should round-trip through FEN key-equivalently.
# Use combined potion+move UCI tokens so the test actually exercises potion state.
assert_reload_key_match "" "spell-chess" "position startpos moves f@a6,e2e4 j@a7,a8a2"
assert_progressive_reload_keys "" "spell-chess" "position startpos" 6
assert_reload_perft1_match "" "spell-chess" "position startpos moves f@a6,e2e4 j@a7,a8a2"
assert_reload_perft1_moves_match "" "spell-chess" "position startpos moves f@a6,e2e4 j@a7,a8a2"
assert_reload_eval_match "" "spell-chess" "position startpos moves f@a6,e2e4 j@a7,a8a2"

echo "state-sync key tests OK"
