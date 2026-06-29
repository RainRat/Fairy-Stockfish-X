#!/usr/bin/env bash
set -euo pipefail

error() {
  echo "spell potion movegen test failed on line $1" >&2
  exit 1
}
trap 'error ${LINENO}' ERR

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ROOT_DIR=$(cd -- "${SCRIPT_DIR}/.." && pwd)
CXX=${CXX:-g++}
JOBS=${JOBS:-2}
ENGINE=${1:-./stockfish}
ENGINE_BASENAME=$(basename "${ENGINE}")
CXX_DEFS=(-DIS_64BIT -DUSE_PTHREADS)
case "${ENGINE_BASENAME}" in
  stockfish-allvars*)
    CXX_DEFS+=(-DLARGEBOARDS -DPRECOMPUTED_MAGICS -DALLVARS -DNNUE_EMBEDDING_OFF)
    ;;
  stockfish-large*)
    CXX_DEFS+=(-DLARGEBOARDS -DPRECOMPUTED_MAGICS -DALLVARS -DNNUE_EMBEDDING_OFF)
    ;;
  stockfish-vlb*)
    CXX_DEFS+=(-DLARGEBOARDS -DVERY_LARGE_BOARDS -DALLVARS -DNNUE_EMBEDDING_OFF)
    ;;
esac

BUILD_SIG_DIR="${ROOT_DIR}/.local/build/spell-potion-movegen"
BUILD_SIG_FILE="${BUILD_SIG_DIR}/${ENGINE_BASENAME}.sig"
HARNESS_CPP="${BUILD_SIG_DIR}/spell-potion-movegen.cpp"
HARNESS_BIN="${BUILD_SIG_DIR}/spell-potion-movegen.bin"
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
if [[ ! -f "${BUILD_SIG_FILE}" || "$(cat "${BUILD_SIG_FILE}" 2>/dev/null || true)" != "${BUILD_SIG}" ]]; then
  printf '%s\n' "${BUILD_SIG}" > "${BUILD_SIG_FILE}"
fi

cat > "${HARNESS_CPP}" <<'EOF'
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>

#include "bitboard.h"
#include "endgame.h"
#include "movegen.h"
#include "piece.h"
#include "position.h"
#include "psqt.h"
#include "uci.h"
#include "variant.h"

using namespace Stockfish;

static void init_engine() {
    UCI::init(Options);
    pieceMap.init();
    variants.init();
    PSQT::init(variants.get("fairy"));
    Bitboards::init();
    Position::init();
    Bitbases::init();
    Endgames::init();
}

static void expect(bool cond, const char* msg) {
    if (!cond) {
        std::cerr << msg << std::endl;
        std::exit(1);
    }
}

static void load_test_variants() {
    static bool loaded = false;
    if (loaded)
        return;

    std::istringstream in(R"INI(
[commitgate-test:chess]
commitGates = true
castling = false
startFen = n7/4k3/8/8/8/8/8/8/4K3/4R3 w - - 0 1
)INI");
    variants.parse_istream<false>(in);
    loaded = true;
}

static void test_jump_lists() {
    StateInfo st{};
    Position pos;
    pos.set(variants.get("spell-chess"), "7k/8/8/p7/8/p7/8/R3K3[J] w - - 0 1", false, &st, nullptr);

    std::string quietJumpStr = "j@a3,a1a4";
    std::string captureJumpStr = "j@a3,a1a5";
    const Move quietJump = UCI::to_move(pos, quietJumpStr);
    const Move captureJump = UCI::to_move(pos, captureJumpStr);
    expect(quietJump != MOVE_NONE, "jump potion quiet move failed to parse");
    expect(captureJump != MOVE_NONE, "jump potion capture failed to parse");

    const auto legalMoves = MoveList<LEGAL>(pos);

    expect(legalMoves.contains(quietJump), "jump potion quiet move missing from LEGAL");
    expect(legalMoves.contains(captureJump), "jump potion capture missing from LEGAL");
}

static void test_jump_checks() {
    StateInfo st{};
    Position captureCheckPos;
    captureCheckPos.set(variants.get("spell-chess"), "7K/8/8/8/8/8/8/R1p3pk[J] w - - 0 1", false, &st, nullptr);
    std::string captureCheckStr = "j@c1,a1g1";
    const Move captureCheck = UCI::to_move(captureCheckPos, captureCheckStr);
    expect(captureCheck != MOVE_NONE, "jump potion checking capture failed to parse");

    const auto legalMoves = MoveList<LEGAL>(captureCheckPos);

    expect(legalMoves.contains(captureCheck), "jump potion checking capture missing from LEGAL");
}

static void test_empty_destination_capture_predicate() {
    StateInfo st{};
    Position pos;
    pos.set(variants.get("spell-chess"), "4k3/8/8/3pP3/8/8/8/4K3[F] w - d6 0 1", false, &st, nullptr);

    const Move epCapture = make<EN_PASSANT>(SQ_E5, SQ_D6);
    const bool oldStyleCapture = !pos.empty(to_sq(epCapture));
    const bool newStyleCapture = pos.capture(epCapture);
    expect(newStyleCapture, "en passant base move should be a capture");
    expect(pos.empty(to_sq(epCapture)), "en passant destination should be empty");
    expect(oldStyleCapture != newStyleCapture, "empty-destination capture did not expose the misclassification condition");
}

static void test_jump_evasions() {
    StateInfo st{};
    Position pos;
    pos.set(variants.get("spell-chess"), "k6r/8/8/8/8/8/8/R1p4K[J] w - - 0 1", false, &st, nullptr);
    std::string nonEvasionStr = "j@c1,a1g1";
    const Move nonEvasion = UCI::to_move(pos, nonEvasionStr);
    expect(nonEvasion != MOVE_NONE, "non-evasion jump potion move failed to parse");

    const auto legalMoves = MoveList<LEGAL>(pos);
    expect(legalMoves.contains(nonEvasion), "non-evasion jump potion move missing from LEGAL");
}

static void test_committed_piece_type_helpers() {
    StateInfo st{};
    const Variant* v = variants.get("commitgate-test");
    expect(v != nullptr, "commitgate-test variant failed to load");

    Position pos;
    pos.set(v, "n7/4k3/8/8/8/8/8/8/4K3/4R3 w - - 0 1", false, &st, nullptr);

    const Move fakeCastling = make<CASTLING>(SQ_E1, SQ_A8);
    expect(pos.committed_piece_type(fakeCastling, false) == ROOK, "king-side committed piece lookup failed");
    expect(pos.committed_piece_type(fakeCastling, true) == KNIGHT, "rook-side committed piece lookup failed");
    expect(pos.committed_piece_type(make<NORMAL>(SQ_E2, SQ_E4), true) == NO_PIECE_TYPE, "non-castling lookup should be empty");
}

static void test_committed_gate_overflow_parse() {
    StateInfo st{};
    const Variant* v = variants.get("commitgate-test");
    expect(v != nullptr, "commitgate-test variant failed to load");

    Position pos;
    pos.set(v, "nnnnnnnnn/4k3/8/8/8/8/8/8/4K3/RRRRRRRRR w - - 0 1", false, &st, nullptr);
    expect(pos.pos_is_ok(), "overlong committed-gate FEN did not parse cleanly");
}

int main() {
    init_engine();
    load_test_variants();
    test_jump_lists();
    test_jump_checks();
    test_empty_destination_capture_predicate();
    test_jump_evasions();
    test_committed_piece_type_helpers();
    test_committed_gate_overflow_parse();
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

echo "spell potion movegen test started"
"${HARNESS_BIN}"
echo "spell potion movegen test passed"
