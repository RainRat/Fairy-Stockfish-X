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
BUILD_SIG="$(printf '%s|%s|%s|%s\n' \
    "${ENGINE_BASENAME}" \
    "${CXX}" \
    "${MAKEFILE_HASH}" \
    "${CXX_DEFS[*]}")"
NEEDS_REBUILD=1
if [[ -f "${BUILD_SIG_FILE}" ]] && [[ "$(cat "${BUILD_SIG_FILE}")" == "${BUILD_SIG}" ]]; then
  NEEDS_REBUILD=0
fi

case "${ENGINE_BASENAME}" in
  stockfish)
    # The harness links against the object files in src/. Rebuild the standard
    # object set so the test is independent of any previous largeboard build.
    if [[ "${NEEDS_REBUILD}" -eq 1 ]]; then
      make -C "${ROOT_DIR}/src" EXE=stockfish objclean
    fi
    make -C "${ROOT_DIR}/src" -j"${JOBS}" build ARCH=x86-64 EXE=stockfish
    ;;
  stockfish-allvars*)
    if [[ "${NEEDS_REBUILD}" -eq 1 ]]; then
      make -C "${ROOT_DIR}/src" EXE=stockfish-allvars objclean
    fi
    make -C "${ROOT_DIR}/src" -j"${JOBS}" build ARCH=x86-64 largeboards=yes all=yes nnue=yes EXE=stockfish-allvars
    ;;
  stockfish-large*)
    if [[ "${NEEDS_REBUILD}" -eq 1 ]]; then
      make -C "${ROOT_DIR}/src" EXE=stockfish-large objclean
    fi
    make -C "${ROOT_DIR}/src" -j"${JOBS}" build ARCH=x86-64 largeboards=yes all=yes EXE=stockfish-large
    ;;
  stockfish-vlb*)
    if [[ "${NEEDS_REBUILD}" -eq 1 ]]; then
      make -C "${ROOT_DIR}/src" EXE=stockfish-vlb objclean
    fi
    make -C "${ROOT_DIR}/src" -j"${JOBS}" build ARCH=x86-64 largeboards=yes verylargeboards=yes all=yes nnue=yes EXE=stockfish-vlb
    ;;
esac
printf '%s\n' "${BUILD_SIG}" > "${BUILD_SIG_FILE}"

cat > "${HARNESS_CPP}" <<'EOF'
#include <cassert>
#include <cstring>
#include <sstream>

#include "bitboard.h"
#include "endgame.h"
#include "movegen.h"
#include "piece.h"
#include "position.h"
#include "psqt.h"
#include "variant.h"

using namespace Stockfish;

static void load_variants() {
    std::istringstream ss(R"ini(
[pairdrop:fairy]
pieceDrops = true
symmetricDropTypes = r

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

static void init_engine() {
    pieceMap.init();
    variants.init();
    PSQT::init(variants.get("fairy"));
    Bitboards::init();
    Position::init();
    Bitbases::init();
    Endgames::init();
    load_variants();
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

int main(int argc, char** argv) {
    init_engine();

    auto run_case = [&](const char* which) {
        if (!which || !std::strcmp(which, "pairdrop"))
            test_pairdrop();
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
    "${CXX}" -std=c++17 -O2 -Wall -Wextra -flto -I"${ROOT_DIR}/src" "${CXX_DEFS[@]}" "${HARNESS_CPP}" "${OBJ_FILES[@]}" -pthread -o "${HARNESS_BIN}"
  )
  printf '%s\n' "${HARNESS_SIG}" > "${HARNESS_SIG_FILE}"
fi

run_case() {
  local which="$1"
  "${HARNESS_BIN}" "${which}"
}

run_case pairdrop
run_case swap
run_case pull
run_case wrapped

echo "quiet-check-special-moves ok"
