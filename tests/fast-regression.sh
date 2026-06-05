#!/bin/bash

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
export ROOT_DIR
source "${ROOT_DIR}/tests/lib/uci.sh"

ENGINE=${1:-src/stockfish}
PYTHON=${PYTHON:-python3}
JOBS=${JOBS:-2}
export JOBS
VARIANT_PATH="${VARIANT_PATH:-${ROOT_DIR}/src/variants.ini}"
VARIANTS="${VARIANTS:-${VARIANT_PATH}}"
export VARIANTS
PYFFISH_BUILD_DIR="${ROOT_DIR}/.local/build/pyffish"
PYFFISH_SIG_FILE="${PYFFISH_BUILD_DIR}/fast-regression.sig"

run_step() {
  local label="$1"
  shift
  echo "== ${label} =="
  TIMEFORMAT='elapsed %3R s'
  time "$@"
}

TEMP_LOG_DIR=""
PIDS=()
LABELS=()
LOGS=()

setup_parallel() {
  TEMP_LOG_DIR=$(mktemp -d "${TMPDIR:-/tmp}/fast-regression-logs-XXXXXX")
}

cleanup_parallel() {
  if [[ -n "${TEMP_LOG_DIR:-}" && -d "${TEMP_LOG_DIR}" ]]; then
    rm -rf "${TEMP_LOG_DIR}"
  fi
}

trap cleanup_parallel EXIT

run_step_bg() {
  local label="$1"
  shift
  local safe_label="${label//[^a-zA-Z0-9_]/_}"
  local log_file="${TEMP_LOG_DIR}/${safe_label}.log"

  (
    echo "== ${label} =="
    /usr/bin/time -f "elapsed %es" "$@"
  ) > "${log_file}" 2>&1 &

  PIDS+=($!)
  LABELS+=("${label}")
  LOGS+=("${log_file}")
}

wait_all() {
  local exit_code=0
  for i in "${!PIDS[@]}"; do
    local pid="${PIDS[$i]}"
    local label="${LABELS[$i]}"
    local log="${LOGS[$i]}"

    if ! wait "$pid"; then
      exit_code=1
    fi
    cat "$log"
  done

  if [[ $exit_code -ne 0 ]]; then
    echo "fast regression suite failed"
    exit 1
  fi
}

hash_file() {
  local path="$1"
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$path" | awk '{print $1}'
  elif command -v shasum >/dev/null 2>&1; then
    shasum -a 256 "$path" | awk '{print $1}'
  else
    wc -c < "$path" | awk '{print $1}'
  fi
}

hash_source_tree() {
  if command -v sha256sum >/dev/null 2>&1; then
    find src -type f \( -name '*.cpp' -o -name '*.h' \) -print0 \
      | sort -z \
      | xargs -0 sha256sum \
      | sha256sum \
      | awk '{print $1}'
  elif command -v shasum >/dev/null 2>&1; then
    find src -type f \( -name '*.cpp' -o -name '*.h' \) -print0 \
      | sort -z \
      | xargs -0 shasum -a 256 \
      | shasum -a 256 \
      | awk '{print $1}'
  else
    find src -type f \( -name '*.cpp' -o -name '*.h' \) -print0 \
      | sort -z \
      | xargs -0 wc -c \
      | awk '{sum += $1} END {print sum}'
  fi
}

ensure_pyffish_extension() {
  local setup_hash source_hash py_version cxx_version current_sig cached_sig pyffish_so=""

  mkdir -p "${PYFFISH_BUILD_DIR}"

  shopt -s nullglob
  local pyffish_candidates=("${ROOT_DIR}"/pyffish*.so)
  shopt -u nullglob
  if (( ${#pyffish_candidates[@]} > 0 )); then
    pyffish_so="${pyffish_candidates[0]}"
  fi

  setup_hash=$(hash_file "${ROOT_DIR}/setup.py")
  source_hash=$(hash_source_tree)
  py_version=$("${PYTHON}" -V 2>&1)
  cxx_version=$("${CXX:-g++}" --version | head -n1)
  current_sig=$(printf '%s|%s|%s|%s\n' "${setup_hash}" "${source_hash}" "${py_version}" "${cxx_version}")

  if [[ -f "${PYFFISH_SIG_FILE}" ]] && [[ -n "${pyffish_so}" ]]; then
    cached_sig=$(<"${PYFFISH_SIG_FILE}")
    if [[ "${cached_sig}" == "${current_sig}" ]]; then
      return
    fi
  fi

  if [[ -n "${pyffish_so}" ]] && [[ "${ROOT_DIR}/setup.py" -ot "${pyffish_so}" ]]; then
    if ! find src -type f \( -name '*.cpp' -o -name '*.h' \) -newer "${pyffish_so}" -print -quit | grep -q .; then
      printf '%s\n' "${current_sig}" > "${PYFFISH_SIG_FILE}"
      return
    fi
  fi

  run_step "pyffish extension" timeout 10m "${PYTHON}" setup.py build_ext --inplace --build-temp "${PYFFISH_BUILD_DIR}"
  printf '%s\n' "${current_sig}" > "${PYFFISH_SIG_FILE}"
}

prepare_fast_inline_variants() {
  FSX_TMP_INI=$(mktemp "${TMPDIR:-/tmp}/fsx-fast-variants-XXXXXX.ini")
  export FSX_TMP_INI
  TMP_VARIANTS="${FSX_TMP_INI}"
  export TMP_VARIANTS
  cat >"${FSX_TMP_INI}" <<'INI'
[istep-piece-specific:chess]
king = -
checking = false
customPiece1 = a:iW
pieceToCharTable = A:a
startFen = 8/8/8/8/8/8/8/4A3 w - - 0 1
doubleStepRegionWhite = A(e1); *(*2)

[irider-piece-specific:chess]
king = -
checking = false
customPiece1 = a:imR2
pieceToCharTable = A:a
startFen = 8/8/8/8/8/8/8/4A3 w - - 0 1
doubleStepRegionWhite = A(e1); *(*2)

[itriple-piece-specific:chess]
king = -
checking = false
customPiece1 = a:iW
pieceToCharTable = A:a
startFen = 8/8/8/8/8/8/8/4A3 w - - 0 1
tripleStepRegionWhite = A(e1)

[ipawnlike-piece-specific:chess]
customPiece1 = a:iW
pieceToCharTable = A:a
pawnLikeTypes = a
startFen = 4k3/8/8/8/8/8/8/4A2K w - - 0 1
doubleStepRegionWhite = A(e1); *(*2)

[irider-roundtrip:chess]
king = k
customPiece1 = d:efWfFmsWifmnD
pieceToCharTable = PNBRQ............D...Kpnbrq............d...k
pawnLikeTypes = d
enPassantTypes = d
startFen = 4k3/8/8/8/8/8/8/4D2K w - - 0 1
doubleStepRegionWhite = D(e1); *(*2)

[semitorpedo-test:chess]
doubleStepRegionWhite = *2 *3
doubleStepRegionBlack = *7 *6
startFen = rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1

[pawnlike-nonstep:chess]
customPiece1 = m:NN
pieceToCharTable = PNBRQ............M...Kpnbrq............m...k
pawnLikeTypes = m
startFen = 4k3/8/8/8/8/8/M7/K7 w - - 0 1

[immobility-illegal-hopper-test:chess]
maxFile = h
maxRank = 8
pieceDrops = true
immobilityIllegal = true
king = k:W
customPiece1 = m:fpR
customPiece2 = g:W
promotedPieceType = m:g
startFen = 8/8/8/8/8/8/8/4K3[M]

[same-player-repeat-control:chess]
startFen = 4k3/8/8/8/8/8/R7/4K3 w - - 0 1

[same-player-repeat-illegal:same-player-repeat-control]
samePlayerBoardRepetitionIllegal = true

[flip5:chess]
maxRank = 5
maxFile = e
startFen = 4k/5/5/5/4K w - - 0 1

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
maxRank = 5
maxFile = 5
castling = false
doubleStep = false
startFen = 4k/5/5/5/4K w - - 0 1
customPiece1 = a:NN
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k

[dabbaba-edge:chess]
maxRank = 5
maxFile = 5
castling = false
doubleStep = false
startFen = 4k/5/5/5/4K w - - 0 1
customPiece1 = a:DD
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k

[alfil-edge:chess]
maxRank = 5
maxFile = 5
castling = false
doubleStep = false
startFen = 4k/5/5/5/4K w - - 0 1
customPiece1 = a:AA
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k

[griffon-edge:chess]
maxRank = 5
maxFile = 5
castling = false
doubleStep = false
startFen = 4k/5/5/5/4K w - - 0 1
customPiece1 = a:O
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k

[manticore-edge:chess]
maxRank = 5
maxFile = 5
castling = false
doubleStep = false
startFen = 4k/5/5/5/4K w - - 0 1
customPiece1 = a:M
pieceToCharTable = PNBRQ............A...Kpnbrq............a...k

[v1:chess]
startFen = 4k3/8/8/8/8/8/8/4K3 w - - 0 1

[v2:v1]
startFen = 4k3/8/8/8/4P3/8/8/4K3 w - - 0 1
INI
}

ensure_fast_inline_variants() {
  if [[ -z "${FSX_TMP_INI:-}" || ! -f "${FSX_TMP_INI}" ]]; then
    prepare_fast_inline_variants
  fi
}

test_janggi_regression() {
  if ! variant_available "$ENGINE" janggi "$VARIANTS"; then
    echo "janggi variant not available in this build; skipping janggi regression"
    return 0
  fi

  local out
  out=$(run_uci "$ENGINE" "$VARIANTS" janggi <<'EOF'
position startpos
go perft 1
EOF
)
  assert_contains "${out}" "^Nodes searched: 32$"
  assert_contains "${out}" "^0000: 1$"

out=$(run_uci "$ENGINE" "$VARIANTS" janggi <<'EOF'
position fen 1n1kaabn1/cr2N4/5C1c1/p1pNp3p/9/9/P1PbP1P1P/3r1p3/4A4/R1BA1KB1R b - - 0 1 moves a9e9 e2d3
go perft 1
EOF
)
assert_contains "${out}" "^Nodes searched: 37$"
assert_contains "${out}" "^f3e2: 1$"
assert_contains "${out}" "^0000: 1$"
}

test_piece_specific_step_regions() {
  ensure_fast_inline_variants
  local out

  out=$(run_uci "$ENGINE" "$TMP_VARIANTS" istep-piece-specific <<'UCI'
position startpos
go perft 1
UCI
)
  assert_contains "$out" "^e1e2: 1$"
  assert_contains "$out" "^e1e3: 1$"
  assert_not_contains "$out" "^e1e4: 1$"

  out=$(run_uci "$ENGINE" "$TMP_VARIANTS" irider-piece-specific <<'UCI'
position startpos
go perft 1
UCI
)
  assert_contains "$out" "^e1e2: 1$"
  assert_contains "$out" "^e1e3: 1$"

  out=$(run_uci "$ENGINE" "$TMP_VARIANTS" itriple-piece-specific <<'UCI'
position startpos
go perft 1
UCI
)
  assert_contains "$out" "^e1e2: 1$"
  assert_contains "$out" "^e1e3: 1$"
  assert_contains "$out" "^e1e4: 1$"

  out=$(run_uci "$ENGINE" "$TMP_VARIANTS" ipawnlike-piece-specific <<'UCI'
position startpos
go perft 1
UCI
)
  assert_contains "$out" "^e1e2: 1$"
  assert_contains "$out" "^e1e3: 1$"
  assert_not_contains "$out" "^e1e4: 1$"

  out=$(run_uci "$ENGINE" "$TMP_VARIANTS" irider-roundtrip <<'UCI'
position fen 4k3/8/8/8/8/8/8/4D2K w - - 0 1 moves e1d1 e8e7 d1e1 e7e8
go perft 1
UCI
)
  assert_contains "$out" "^e1e2: 1$"
  assert_not_contains "$out" "^e1e4: 1$"

  out=$(run_uci "$ENGINE" "$TMP_VARIANTS" semitorpedo-test <<'UCI'
position startpos moves e2e3 a7a6
go perft 1
UCI
)
  assert_contains "$out" "^e3e4: 1$"
  assert_contains "$out" "^e3e5: 1$"

  out=$(run_uci "$ENGINE" "$TMP_VARIANTS" semitorpedo-test <<'UCI'
position startpos moves e2e4 a7a6
go perft 1
UCI
)
  assert_contains "$out" "^e4e5: 1$"
  assert_not_contains "$out" "^e4e6: 1$"
}

test_pawnlike_custom_nonstep() {
  ensure_fast_inline_variants
  local out
  out=$(printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value pawnlike-nonstep\nposition startpos\ngo perft 1\nquit\n' "$TMP_VARIANTS" \
    | uci_timeout "$ENGINE")
  assert_contains "$out" "^a2c1: 1$"
  assert_contains "$out" "^a2c3: 1$"
  assert_not_contains "$out" "^a2a[34]:"
}

test_variant_switch_after_perft() {
  ensure_fast_inline_variants
  local out
  out=$(run_uci "$ENGINE" "$TMP_VARIANTS" v1 <<'CMDS'
position startpos
go perft 1
setoption name UCI_Variant value v2
CMDS
)
  assert_contains "$out" "^e1d1: 1$"
  assert_contains "$out" "info string variant v2 files 8 ranks 8 pocket 0 template fairy startpos 4k3/8/8/8/4P3/8/8/4K3 w - - 0 1"
}

test_immobility_illegal_hoppers() {
  ensure_fast_inline_variants
  local out
  out=$(run_uci "$ENGINE" "$TMP_VARIANTS" immobility-illegal-hopper-test <<'EOF'
position fen 8/8/8/8/8/8/8/4K3[M] w - - 0 1
go perft 1
EOF
)
  assert_contains "$out" "^M@a6:"
  assert_contains "$out" "^M@e6:"
  assert_not_contains "$out" "^M@a7:"
  assert_not_contains "$out" "^M@e7:"
  assert_not_contains "$out" "^M@a8:"
  assert_not_contains "$out" "^M@e8:"
}

test_same_player_board_repetition() {
  ensure_fast_inline_variants
  local out
  out=$(run_uci "$ENGINE" "$TMP_VARIANTS" same-player-repeat-control <<'CMDS'
position startpos moves a2a3 e8e7 a3a2 e7e8
go perft 1
CMDS
)
  assert_contains "$out" "^a2a3: 1$"

  out=$(run_uci "$ENGINE" "$TMP_VARIANTS" same-player-repeat-illegal <<'CMDS'
position startpos moves a2a3 e8e7 a3a2 e7e8
go perft 1
CMDS
)
  assert_contains "$out" "^e1d1: 1$"
}

test_pushing() {
  ensure_fast_inline_variants
  local out

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

  out=$(run_uci "$ENGINE" "$VARIANT_PATH" aries <<'CMDS'
position fen 8/8/8/Rrrr4/8/8/8/8 w - - 0 1
go perft 1
CMDS
)
  assert_contains "$out" "^a5b5: 1$"
  assert_contains "$out" "^Nodes searched: 8$"

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
  assert_contains "$out" "^Nodes searched: 8$"

  out=$(run_uci "$ENGINE" "$TMP_VARIANTS" push-stepwise-capture <<'CMDS'
position fen 5/5/1R1rR/5/5 w - - 0 1
go perft 2
CMDS
)
  assert_contains "$out" "^Nodes searched: 81$"
}

test_rider_edge_consistency() {
  ensure_fast_inline_variants
  local out

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
go depth 1 searchmoves b2b4
UCI
)
  assert_contains_literal "$out" "bestmove (none)"

  out=$(run_uci "$ENGINE" "$TMP_VARIANTS" manticore-edge <<'UCI'
position fen 1K3/2A2/a4/5/5 w - - 0 1
go depth 1 searchmoves c4a3
UCI
)
  assert_contains "$out" "bestmove c4a3"
  out=$(run_uci "$ENGINE" "$TMP_VARIANTS" manticore-edge <<'UCI'
position fen 1K3/2A2/a4/5/5 w - - 0 1
go depth 1 searchmoves c4b4
UCI
)
  assert_contains_literal "$out" "bestmove (none)"
}

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
  assert_contains "$out" "^Invalid potion cooldown specification in FEN: '<1 2 x 4>'\\.$"
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

cd "${ROOT_DIR}"

if ! printf 'uci\nquit\n' | "${ENGINE}" | grep -q ' var duck'; then
  echo "note: ${ENGINE} does not expose 'duck' in UCI_Variant (likely non-all build); all-only alias coverage is skipped." >&2
fi

ENGINE_BASENAME=$(basename "${ENGINE}")
case "${ENGINE_BASENAME}" in
  stockfish-large*)
    run_step "prep largeboard objects" timeout 30m bash -lc 'cd src && make -s EXE=stockfish-large objclean && make -s -j"${JOBS}" build ARCH=x86-64 largeboards=yes all=yes EXE=stockfish-large'
    ;;
  stockfish-vlb*)
    run_step "prep very-large-board objects" timeout 30m bash -lc 'cd src && make -s EXE=stockfish-vlb objclean && make -s -j"${JOBS}" build ARCH=x86-64 largeboards=yes verylargeboards=yes all=yes EXE=stockfish-vlb'
    ;;
esac

ensure_pyffish_extension
export PYTHONPATH="${ROOT_DIR}${PYTHONPATH:+:${PYTHONPATH}}"

setup_parallel
prepare_fast_inline_variants

run_step "quiet-check special moves" timeout 5m bash tests/quiet-check-special-moves.sh "${ENGINE}"
run_step "gating check regressions" timeout 2m bash tests/gating-check-regression.sh "${ENGINE}"

run_step_bg "passive blast" timeout 60s bash tests/passive-blast.sh "${ENGINE}"
run_step_bg "crazyhouse multi pawn promo" timeout 60s bash tests/crazyhouse-multi-pawn-promo.sh "${ENGINE}"
run_step_bg "binding regression" timeout 60s "${PYTHON}" tests/test_binding_regression.py
run_step_bg "royal capture no kings" timeout 60s "${PYTHON}" tests/test_royal_capture_no_kings.py
if [[ -n "${UPSTREAM_ENGINE:-}" ]]; then
  run_step_bg "upstream movecount baseline" timeout 60s "${PYTHON}" tests/upstream_movecount_baseline.py "${ENGINE}" "${UPSTREAM_ENGINE}"
else
  run_step_bg "upstream movecount baseline" timeout 60s "${PYTHON}" tests/upstream_movecount_baseline.py "${ENGINE}"
fi
run_step_bg "python unit tests" timeout 180s "${PYTHON}" test.py

run_step "janggi regressions" test_janggi_regression
run_step "piece-specific step regions" test_piece_specific_step_regions
run_step "pawn-like custom non-step" test_pawnlike_custom_nonstep
run_step "custom en passant passed squares" test_custom_en_passant_passed_squares
run_step "standard piece value phase" test_standard_piece_value_phase
run_step "flip regressions" test_flip_regressions
run_step "changing-color locality" test_changing_color_locality
run_step "potion check regressions" test_potion_check_regressions
run_step "pousse counting" test_pousse_counting
run_step "repetition loss search" test_repetition_loss_search
run_step "potion custom" test_potion_custom
run_step "variant switch after perft" test_variant_switch_after_perft
run_step "immobility illegal hoppers" test_immobility_illegal_hoppers
run_step "same-player-board repetition" test_same_player_board_repetition
run_step "pushing" test_pushing
run_step "rider edge consistency" test_rider_edge_consistency

wait_all

if [[ -n "${FSX_TMP_INI:-}" && -e "${FSX_TMP_INI}" ]]; then
  rm -f "${FSX_TMP_INI}"
fi
FSX_TMP_INI=
TMP_VARIANTS=

echo "fast regression suite passed"
