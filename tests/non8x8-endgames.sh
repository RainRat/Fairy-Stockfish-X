#!/bin/bash

set -euo pipefail

error() {
  echo "non8x8 endgame test failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
ENGINE="${1:-${REPO_ROOT}/src/stockfish}"

extract_eval() {
  sed -n 's/^Final evaluation[[:space:]]*//p' | tail -n1 | awk '{print $1}'
}

run_eval() {
  local variant_path="$1"
  local variant="$2"
  local fen="$3"
  cat <<CMDS | "${ENGINE}" | extract_eval
uci
setoption name VariantPath value ${variant_path}
setoption name UCI_Variant value ${variant}
position fen ${fen}
eval
quit
CMDS
}

tmp_ini=$(mktemp)
trap 'rm -f "${tmp_ini}"' EXIT

cat > "${tmp_ini}" <<'INI'
[mini-anti:giveaway]
maxFile = 5
maxRank = 5
startFen = 5/5/5/5/K3n w - - 0 1

[mini-rk:racingkings]
maxFile = 5
maxRank = 5
startFen = 5/5/5/5/K3k w - - 0 1

[mini-10x8:chess]
maxFile = 10
maxRank = 8
castling = false
INI

# Non-8x8 anti endgames should remain horizontally symmetric.
anti_left=$(run_eval "${tmp_ini}" "mini-anti" "5/5/K4/5/2n2 w - - 0 1")
anti_right=$(run_eval "${tmp_ini}" "mini-anti" "5/5/4K/5/2n2 w - - 0 1")
[[ "${anti_left}" == "${anti_right}" ]]

anti_inner_left=$(run_eval "${tmp_ini}" "mini-anti" "5/5/1K3/5/2n2 w - - 0 1")
anti_inner_right=$(run_eval "${tmp_ini}" "mini-anti" "5/5/3K1/5/2n2 w - - 0 1")
[[ "${anti_inner_left}" == "${anti_inner_right}" ]]

# Non-8x8 racing-kings evaluators should stay mirror-consistent as well.
rk_queen_left=$(run_eval "${tmp_ini}" "mini-rk" "2k2/1Q3/5/5/K4 w - - 0 1")
rk_queen_right=$(run_eval "${tmp_ini}" "mini-rk" "2k2/3Q1/5/5/4K w - - 0 1")
[[ "${rk_queen_left}" == "${rk_queen_right}" ]]

rk_rook_left=$(run_eval "${tmp_ini}" "mini-rk" "2k2/1R3/5/5/K4 w - - 0 1")
rk_rook_right=$(run_eval "${tmp_ini}" "mini-rk" "2k2/3R1/5/5/4K w - - 0 1")
[[ "${rk_rook_left}" == "${rk_rook_right}" ]]

# Non-8x8 KQKP should not leak VALUE_NONE from the specialized evaluator.
kqkp_10x8=$(run_eval "${tmp_ini}" "mini-10x8" "10/10/10/4k5/10/10/10/4KQp3 w - - 0 1")
[[ -n "${kqkp_10x8}" ]]

# Non-8x8 KPK / KPKP should also skip 8x8 bitbase assumptions cleanly.
kpk_10x8=$(run_eval "${tmp_ini}" "mini-10x8" "9k/10/10/10/10/10/4P5/4K5 w - - 0 1")
[[ -n "${kpk_10x8}" ]]

kpkp_10x8=$(run_eval "${tmp_ini}" "mini-10x8" "9k/10/10/10/10/10/4P4p/4K5 w - - 0 1")
[[ -n "${kpkp_10x8}" ]]

echo "non8x8 endgame test OK"