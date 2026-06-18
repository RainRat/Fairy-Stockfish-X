#!/usr/bin/env bash

set -euo pipefail

error() {
  echo "quiet-check-special-moves regression failed on line $1" >&2
  exit 1
}
trap 'error ${LINENO}' ERR

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ROOT_DIR=$(cd -- "${SCRIPT_DIR}/.." && pwd)
CXX=${CXX:-g++}
JOBS=${JOBS:-2}
ENGINE=${1:-./stockfish}
ENGINE_BASENAME=$(basename "${ENGINE}")
CXX_DEFS=()
case "${ENGINE_BASENAME}" in
  stockfish-allvars*)
    CXX_DEFS+=(-DLARGEBOARDS -DALLVARS -DNNUE_EMBEDDING_OFF)
    ;;
  stockfish-large*)
    CXX_DEFS+=(-DLARGEBOARDS -DALLVARS -DNNUE_EMBEDDING_OFF)
    ;;
  stockfish-vlb*)
    CXX_DEFS+=(-DLARGEBOARDS -DVERY_LARGE_BOARDS -DALLVARS -DNNUE_EMBEDDING_OFF)
    ;;
esac

BUILD_SIG_DIR="${ROOT_DIR}/.local/build/quiet-check-special-moves"
BUILD_SIG_FILE="${BUILD_SIG_DIR}/${ENGINE_BASENAME}.sig"
HARNESS_CPP="${BUILD_SIG_DIR}/quiet-check-special-moves.cpp"
HARNESS_BIN="${BUILD_SIG_DIR}/quiet-check-special-moves.bin"
HARNESS_SIG_FILE="${BUILD_SIG_DIR}/${ENGINE_BASENAME}.harness.sig"
mkdir -p "${BUILD_SIG_DIR}"
if command -v sha256sum >/dev/null 2>&1; then
  MAKEFILE_HASH="$(cd "${ROOT_DIR}/src" && sha256sum Makefile | cut -d' ' -f1)"
elif command -v shasum >/dev/null 2>&1; then
  MAKEFILE_HASH="$(cd "${ROOT_DIR}/src" && shasum -a 256 Makefile | cut -d' ' -f1)"
else
  MAKEFILE_HASH="no-hash-tool"
fi
NEEDS_REBUILD=1
BUILD_SIG="$(printf '%s|%s|%s|%s\n' \
    "${ENGINE_BASENAME}" \
    "${CXX}" \
    "${MAKEFILE_HASH}" \
    "${CXX_DEFS[*]}")"
if [[ -f "${BUILD_SIG_FILE}" ]] && [[ "$(cat "${BUILD_SIG_FILE}")" == "${BUILD_SIG}" ]]; then
  NEEDS_REBUILD=0
fi

case "${ENGINE_BASENAME}" in
  stockfish)
    # The harness links against the object files in src/. Rebuild the standard
    # object set so the test is independent of any previous largeboard build.
    make -C "${ROOT_DIR}/src" EXE=stockfish objclean
    make -C "${ROOT_DIR}/src" -j"${JOBS}" build ARCH=x86-64 EXE=stockfish
    ;;
  stockfish-allvars*)
    make -C "${ROOT_DIR}/src" EXE=stockfish-allvars objclean
    make -C "${ROOT_DIR}/src" -j"${JOBS}" build ARCH=x86-64 largeboards=yes all=yes nnue=yes EXE=stockfish-allvars
    ;;
  stockfish-large*)
    make -C "${ROOT_DIR}/src" EXE=stockfish-large objclean
    make -C "${ROOT_DIR}/src" -j"${JOBS}" build ARCH=x86-64 largeboards=yes all=yes EXE=stockfish-large
    ;;
  stockfish-vlb*)
    make -C "${ROOT_DIR}/src" EXE=stockfish-vlb objclean
    make -C "${ROOT_DIR}/src" -j"${JOBS}" build ARCH=x86-64 largeboards=yes verylargeboards=yes all=yes nnue=yes EXE=stockfish-vlb
    ;;
esac

if [[ ${#CXX_DEFS[@]} -eq 0 && -f "${ROOT_DIR}/src/position.o" ]]; then
  POSITION_O_SIG="$(nm -C "${ROOT_DIR}/src/position.o" 2>/dev/null || true)"
  if ! grep -q 'Position::fen(bool, bool, int, .*unsigned long) const' <<<"${POSITION_O_SIG}"; then
    CXX_DEFS+=(-DLARGEBOARDS -DPRECOMPUTED_MAGICS -DALLVARS -DNNUE_EMBEDDING_OFF)
  fi
fi

BUILD_SIG="$(printf '%s|%s|%s|%s\n' \
    "${ENGINE_BASENAME}" \
    "${CXX}" \
    "${MAKEFILE_HASH}" \
    "${CXX_DEFS[*]}")"
printf '%s\n' "${BUILD_SIG}" > "${BUILD_SIG_FILE}"

cat > "${HARNESS_CPP}" <<'EOF'
#include <cassert>
#include <cstring>
#include <sstream>

#include "bitboard.h"
#include "endgame.h"
#include "movegen.h"
#include "movepick.h"
#include "piece.h"
#include "position.h"
#include "psqt.h"
#include "thread.h"
#include "uci.h"
#include "variant.h"
#include "test_engine_init.hpp"

using namespace Stockfish;

static void load_variants() {
    std::istringstream ss(R"ini(
[pairdrop:fairy]
pieceDrops = true
symmetricDropTypes = r

[pairdrop-nocheck:fairy]
pieceDrops = true
symmetricDropTypes = r
checking = false
allowChecks = false

[pull-basic:fairy]
maxFile = e
maxRank = 5
castling = false
checking = false
king = -
pieceToCharTable = K...A...R...k...b...r...
king = k
customPiece1 = a:mW
customPiece2 = b:mW
customPiece3 = c:mW
pullingStrength = a:3 b:1 c:3
startFen = 5/5/5/5/5 w - - 0 1
[swap-basic:fairy]
maxFile = e
maxRank = 5
castling = false
checking = false
king = -
pieceToCharTable = -
customPiece1 = a:mW
customPiece2 = b:mW
adjacentSwapMoveTypes = a
adjacentSwapRequiresEmptyNeighbor = true
swapNoImmediateReturn = true
startFen = 5/5/5/5/5 w - - 0 1

[pass-quiet-check:fairy]
pass = true
castling = false
startFen = 8/8/8/8/8/8/8/R3K2k w - - 0 1

[wrapped-quiet-check:chess]
cylindrical = true
castling = false
checking = false
startFen = 2k5/8/8/8/8/7p/P7/4K3 w - - 0 1

[promotion-quiet-check:fairy]
castling = false
startFen = 7k/6P1/8/8/8/8/8/7K w - - 0 1
)ini");
    variants.parse_istream<false>(ss);
}

static void test_swap_basic() {
    StateInfo st{};
    Position pos;
    pos.set(variants.get("swap-basic"), "5/5/2Ab1/5/5 w - - 0 1", false, &st, nullptr);
    const Move m = make<SWAP>(SQ_C3, SQ_D3);
    assert(!pos.gives_check(m));
    assert(!MoveList<QUIET_CHECKS>(pos).contains(m));
}

static void test_pairdrop() {
    StateInfo st{};
    Position pos;
    pos.set(variants.get("pairdrop"), "4k3/8/8/8/8/8/8/4K3[RR] w - - 0 1", false, &st, nullptr);
    const Move m = make_drop_pair(SQ_A4, SQ_H4, ROOK, ROOK);
    assert(!pos.gives_check(m));
    assert(!MoveList<QUIET_CHECKS>(pos).contains(m));
}

static void test_probe_nodes() {
    Thread thread(0);
    thread.nodes.store(0);

    StateInfo st{};
    Position pos;
    pos.set(variants.get("pairdrop-nocheck"), "4k3/8/8/8/8/8/8/4K3[RR] w - - 0 1", false, &st, &thread);
    const Move m = make_drop_pair(SQ_A1, SQ_B1, ROOK, ROOK);

    const auto before = thread.nodes.load();
    assert(pos.legal(m));
    assert(!pos.gives_check(m));
    assert(!pos.gives_check(m));
    assert(thread.nodes.load() == before);
    assert(MoveList<LEGAL>(pos).contains(m));
    assert(thread.nodes.load() == before);
}

static void test_pull_basic() {
    StateInfo st{};
    Position pos;
    pos.set(variants.get("pull-basic"), "5/5/2b2/2A2/5 w - - 0 1", false, &st, nullptr);
    const Move m = make_pull(SQ_C2, SQ_D2, SQ_C3);
    assert(!pos.gives_check(m));
    assert(!MoveList<QUIET_CHECKS>(pos).contains(m));
}

static void test_pass_quiet_check() {
    StateInfo st{};
    Position pos;
    pos.set(variants.get("pass-quiet-check"), "8/8/8/8/8/8/8/R3K2k w - - 0 1", false, &st, nullptr);
    const Move m = make<SPECIAL>(SQ_A1, SQ_A1);
    assert(pos.pass(WHITE));
    assert(!MoveList<QUIET_CHECKS>(pos).contains(m));
}

static void test_wrapped_quiet_check() {
    StateInfo st{};
    Position pos;
    pos.set(variants.get("wrapped-quiet-check"), "2k5/8/8/8/8/7p/P7/4K3 w - - 0 1", false, &st, nullptr);
    const Move m = make<NORMAL>(SQ_A2, SQ_A3);
    assert(!pos.gives_check(m));
    assert(!MoveList<QUIET_CHECKS>(pos).contains(m));
}

static void test_promotion_quiet_check() {
    StateInfo st{};
    Position pos;
    pos.set(variants.get("promotion-quiet-check"), "7k/6P1/8/8/8/8/8/7K w - - 0 1", false, &st, nullptr);
    const auto quietChecks = MoveList<QUIET_CHECKS>(pos);
    assert(pos.gives_check(make<PROMOTION>(SQ_G7, SQ_G8, QUEEN)));
    assert(pos.gives_check(make<PROMOTION>(SQ_G7, SQ_G8, ROOK)));
    assert(!pos.gives_check(make<PROMOTION>(SQ_G7, SQ_G8, BISHOP)));
    assert(!pos.gives_check(make<PROMOTION>(SQ_G7, SQ_G8, KNIGHT)));
    assert(quietChecks.contains(make<PROMOTION>(SQ_G7, SQ_G8, QUEEN)));
    assert(quietChecks.contains(make<PROMOTION>(SQ_G7, SQ_G8, ROOK)));
    assert(!quietChecks.contains(make<PROMOTION>(SQ_G7, SQ_G8, BISHOP)));
    assert(!quietChecks.contains(make<PROMOTION>(SQ_G7, SQ_G8, KNIGHT)));
}

static void test_qsearch_rejects_quiet_tt_move() {
    StateInfo st{};
    Position pos;
    pos.set(variants.get("chess"),
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
            false, &st, nullptr);

    ButterflyHistory mainHistory{};
    GateHistory gateHistory{};
    CapturePieceToHistory captureHistory{};
    PieceToHistory histories[6]{};
    const PieceToHistory* continuationHistory[] = {
        &histories[0], &histories[1], &histories[2],
        &histories[3], &histories[4], &histories[5]
    };

    MovePicker picker(pos, make<NORMAL>(SQ_E2, SQ_E4), DEPTH_QS_NO_CHECKS,
                      &mainHistory, &gateHistory, &captureHistory,
                      continuationHistory, SQ_NONE);
    assert(picker.next_move() == MOVE_NONE);
}

static void test_probcut_accepts_quiet_promotion_tt_move() {
    StateInfo st{};
    Position pos;
    pos.set(variants.get("promotion-quiet-check"),
            "7k/6P1/8/8/8/8/8/7K w - - 0 1", false, &st, nullptr);

    GateHistory gateHistory{};
    CapturePieceToHistory captureHistory{};
    const Move promotion = make<PROMOTION>(SQ_G7, SQ_G8, QUEEN);
    MovePicker picker(pos, promotion, Value(100), &gateHistory, &captureHistory);
    assert(picker.next_move() == promotion);
}

static void test_gate_history_square_validation() {
    const Move valid = make_gating<NORMAL>(SQ_E1, SQ_E2, ROOK, SQ_D1);
    assert(gate_history_square(valid) == SQ_D1);

    const Move ordinary = make<NORMAL>(SQ_E2, SQ_E4);
    assert(!is_gating(ordinary));
    assert(gate_history_square(ordinary) == SQ_NONE);
}

int main(int argc, char** argv) {
    init_test_engine();
    load_variants();

    auto run_case = [&](const char* which) {
        if (!which || !std::strcmp(which, "pairdrop"))
            test_pairdrop();
        if (!which || !std::strcmp(which, "probe-nodes"))
            test_probe_nodes();
        if (!which || !std::strcmp(which, "swap"))
            test_swap_basic();
        if (!which || !std::strcmp(which, "pull"))
            test_pull_basic();
        if (!which || !std::strcmp(which, "pass"))
            test_pass_quiet_check();
        if (!which || !std::strcmp(which, "wrapped"))
            test_wrapped_quiet_check();
        if (!which || !std::strcmp(which, "promotion"))
            test_promotion_quiet_check();
        if (!which || !std::strcmp(which, "qsearch-tt"))
            test_qsearch_rejects_quiet_tt_move();
        if (!which || !std::strcmp(which, "probcut-promotion"))
            test_probcut_accepts_quiet_promotion_tt_move();
        if (!which || !std::strcmp(which, "gate-history-square"))
            test_gate_history_square_validation();
    };

    if (argc > 1)
        run_case(argv[1]);
    else
        run_case(nullptr);

    return 0;
}
EOF

OBJ_FILES=()
while IFS= read -r -d '' obj; do
  OBJ_FILES+=("${obj}")
done < <(find "${ROOT_DIR}/src" -maxdepth 1 -name '*.o' ! -name 'main.o' -print0 | sort -z)

if command -v sha256sum >/dev/null 2>&1; then
  HASHER=(sha256sum)
elif command -v shasum >/dev/null 2>&1; then
  HASHER=(shasum -a 256)
else
  HASHER=()
fi

hash_text() {
  if [[ ${#HASHER[@]} -gt 0 ]]; then
    printf '%s' "$1" | "${HASHER[@]}" | awk '{print $1}'
  else
    printf '%s' "$1" | wc -c | awk '{print $1}'
  fi
}

object_signature() {
  local sig
  sig=$(
    for obj in "${OBJ_FILES[@]}"; do
      stat -c '%n %Y %s' "${obj}" 2>/dev/null || stat -f '%N %m %z' "${obj}"
    done
  )
  hash_text "${sig}"
}

HARNESS_SIG="$(printf '%s|%s|%s|%s|%s|%s\n' \
    "${ENGINE_BASENAME}" \
    "${CXX}" \
    "${MAKEFILE_HASH}" \
    "${CXX_DEFS[*]}" \
    "$(object_signature)" \
    "$(hash_text "$(cat "${HARNESS_CPP}")")")"
if [[ ! -x "${HARNESS_BIN}" || ! -f "${HARNESS_SIG_FILE}" || "$(cat "${HARNESS_SIG_FILE}")" != "${HARNESS_SIG}" ]]; then
  rm -f "${HARNESS_BIN}"
  (
    cd "${ROOT_DIR}/src"
    "${CXX}" -std=c++17 -O2 -Wall -Wextra -flto -I"${ROOT_DIR}/src" -I"${ROOT_DIR}/tests/lib" "${CXX_DEFS[@]}" "${HARNESS_CPP}" "${OBJ_FILES[@]}" -pthread -o "${HARNESS_BIN}"
  )
  printf '%s\n' "${HARNESS_SIG}" > "${HARNESS_SIG_FILE}"
fi

run_case() {
  local which="$1"
  "${HARNESS_BIN}" "${which}"
}

run_raw_cmds() {
  local ini="$1"
  local variant="$2"
  local cmds="$3"
  printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value %s\n%s\nquit\n' \
    "$ini" "$variant" "$cmds" | "${ENGINE}" 2>&1
}

test_passive_blast() {
  local tmp_ini out
  tmp_ini=$(mktemp)
  cat > "${tmp_ini}" <<'INI'
[passive-blast-test:chess]
customPiece1 = f:W
blastPassiveTypes = f
pieceToCharTable = ...............F....K...............f....k
INI

  out=$(run_raw_cmds "${tmp_ini}" passive-blast-test $'position fen 4k3/8/8/4f3/8/4K3/8/8 w - - 0 1\ngo perft 1')
  if grep -q "Can not use kings with blastPassiveTypes." <<<"${out}" || \
     grep -q "unknown variant 'passive-blast-test'" <<<"${out}"; then
    echo "skip: passive blast regression not supported by this build"
    rm -f "${tmp_ini}"
    return 0
  fi
  ! grep -q "^e3e4: 1$" <<<"${out}"

  out=$(run_raw_cmds "${tmp_ini}" passive-blast-test $'position fen r3k3/4f3/8/8/8/8/4K3/8 b - - 0 1\ngo perft 1')
  ! grep -q "^a8a7: 1$" <<<"${out}"

  out=$(run_raw_cmds "${tmp_ini}" passive-blast-test $'position fen r3k3/8/4f3/8/8/8/4K3/8 w - - 0 1 moves e6e7\ngo perft 1')
  ! grep -q "^a8a7: 1$" <<<"${out}"

  out=$(run_raw_cmds "${tmp_ini}" passive-blast-test $'position fen 4k3/8/8/4f3/8/4R3/4K3/8 w - - 0 1 moves e3e4\nd')
  grep -q "Fen: 4k3/8/8/4f3/8/8/4K3/8 b - - 1 1" <<<"${out}"

  cat > "${tmp_ini}" <<'INI'
[passive-blast-immune:chess]
customPiece1 = f:W
blastPassiveTypes = f
blastImmuneTypes = r
pieceToCharTable = ....R..........F....K....r..........f....k
INI
  out=$(run_raw_cmds "${tmp_ini}" passive-blast-immune $'position fen 4k3/8/8/4f3/8/4R3/4K3/8 w - - 0 1 moves e3e4\nd')
  grep -q "Fen: 4k3/8/8/4f3/4R3/8/4K3/8 b - - 1 1" <<<"${out}"

  rm -f "${tmp_ini}"
}

test_crazyhouse_multi_pawn_promo() {
  local tmp_ini out
  tmp_ini=$(mktemp "${TMPDIR:-/tmp}/fsx-crazyhouse-multi-pawn-promo-XXXXXX.ini")
  cat >"${tmp_ini}" <<'VAR'
[newvariant:crazyhouse]
promotionPawnTypes=pb
promotionPieceTypes=qn
VAR

  out=$(run_raw_cmds "${tmp_ini}" newvariant $'position fen r7/7P/8/8/8/8/8/k1K5 w - - 0 1 moves h7h8q a8h8\nd')
  grep -Fq "Fen: 7r/8/8/8/8/8/8/k1K5[p] w - - 0 2" <<<"${out}"
  rm -f "${tmp_ini}"
}

run_case pairdrop
run_case swap
run_case pull
run_case wrapped
run_case gate-history-square
test_passive_blast
test_crazyhouse_multi_pawn_promo

echo "quiet-check-special-moves ok"
