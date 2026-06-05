#!/usr/bin/env bash
# Regression tests for gating check detection and symmetric gating

set -euo pipefail

error() {
  echo "gating-check-regression testing failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-./src/stockfish}
ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
CXX=${CXX:-g++}
JOBS=${JOBS:-2}
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

BUILD_CACHE_DIR="${ROOT_DIR}/.local/build/gating-check-regression"
mkdir -p "${BUILD_CACHE_DIR}"

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

engine_signature() {
  if [[ -e "${ENGINE}" ]]; then
    if [[ ${#HASHER[@]} -gt 0 ]]; then
      "${HASHER[@]}" "${ENGINE}" | awk '{print $1}'
    else
      stat -c '%Y %s' "${ENGINE}" 2>/dev/null || echo "0 0"
    fi
  else
    echo "0 0"
  fi
}

build_signature() {
  local label="$1"
  local source="$2"
  printf '%s|%s|%s|%s|%s|%s\n' \
    "${label}" \
    "${CXX}" \
    "${ENGINE_BASENAME}" \
    "${CXX_DEFS[*]}" \
    "$(engine_signature)" \
    "$(hash_text "${source}")"
}

run_cmds() {
  local ini=$1
  local cmds=$2
  printf "uci\nsetoption name VariantPath value ${ini}\n${cmds}\nquit\n" | "${ENGINE}"
}

echo "Running gating-check-regression tests..."

TMP_INI=$(mktemp "${TMPDIR:-/tmp}/gating-check-XXXXXX.ini")
TMP_CPP="${BUILD_CACHE_DIR}/gating-check-1.cpp"
TMP_BIN="${BUILD_CACHE_DIR}/gating-check-1.bin"
HARNESS_CPP="${BUILD_CACHE_DIR}/gating-check-roundtrip.cpp"
HARNESS_BIN="${BUILD_CACHE_DIR}/gating-check-roundtrip.bin"
trap 'rm -f "${TMP_INI}"' EXIT

case "${ENGINE_BASENAME}" in
  stockfish)
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

# --- TEST 1: Gated piece blocking discovered check ---
cat > "${TMP_INI}" <<'EOF'
[gatingblock:seirawan]
EOF

# White Rook a1, White King e1. Enemy Black King h1.
# Move King e1-e2 and gate a Pawn (p) on e1.
# The gated Pawn at e1 MUST block the discovered check from Rook a1 to King h1.
OUT=$(run_cmds "${TMP_INI}" "setoption name UCI_Variant value gatingblock
position fen 8/8/8/8/8/8/8/R3K2k[P] w KQ - 0 1 moves e1e2p
d")

if echo "$OUT" | grep -q "Checkers: [a-h][1-8]"; then
    echo "FAILED: Engine thinks it is check, but it should be blocked by the gated piece!"
    exit 1
fi

# --- TEST 2: Castling gating occupancy (prevent false positive check) ---
# White King e1, Rook h1. Enemy Black King a1.
# Gate a Rook (R) on h1 while castling kingside (e1g1).
# Gated Rook at h1 would give check to a1 IF g1 and f1 were empty.
# But King at g1 and Rook at f1 must block this check.
OUT=$(run_cmds "${TMP_INI}" "setoption name UCI_Variant value gatingblock
position fen 8/8/8/8/8/8/8/k3K2R[R] w KQBCDEFGH - 0 1 moves e1g1rh1
d")

if echo "$OUT" | grep -q "Checkers: [a-h][1-8]"; then
    echo "FAILED: Engine thinks it is check, but it should be blocked by castled pieces!"
    exit 1
fi

# --- TEST 3: Symmetric Gating Generation ---
cat > "${TMP_INI}" <<'EOF'
[symgating:chess]
gating = true
seirawanGating = true
symmetricDropTypes = r
EOF

# White King e1, pocket has [RR].
# Standard moves: e1d1, e1f1, e1d2, e1e2, e1f2 (5 moves)
# Gating moves: e1f1r,d1; e1d2r,d1; e1e2r,d1; e1f2r,d1 (4 moves). e1d1r is illegal as King is on d1.
# Total: 9 moves.
OUT=$(run_cmds "${TMP_INI}" "setoption name UCI_Variant value symgating
position fen 4k3/8/8/8/8/8/8/4K3[RR] w ABCDEFGH - 0 1
go perft 1")

NODES=$(echo "$OUT" | grep "Nodes searched:" | awk '{print $3}')
if [ "$NODES" -ne 9 ]; then
    echo "FAILED: Expected 9 nodes for symmetric gating, found $NODES"
    exit 1
fi

# --- TEST 4: Encoded gate/pull squares must preserve the last board square ---
TMP_CPP="${BUILD_CACHE_DIR}/gating-check-1.cpp"
TMP_BIN="${BUILD_CACHE_DIR}/gating-check-1.bin"
cat > "${TMP_CPP}" <<'EOF'
#include <cassert>
#include "types.h"

using namespace Stockfish;

int main() {
    assert(gating_square(make_gating<NORMAL>(SQ_A1, SQ_A2, NO_PIECE_TYPE, SQ_H8)) == SQ_H8);
    assert(gating_square(make_gating<CASTLING>(SQ_E1, SQ_G1, ROOK, SQ_H8)) == SQ_H8);
    assert(pull_square(make_pull(SQ_A1, SQ_A2, SQ_H8)) == SQ_H8);
    assert(is_gating(make_gating<NORMAL>(SQ_A1, SQ_A2, NO_PIECE_TYPE, SQ_H8)));
    return 0;
}
EOF
TMP_SIG_FILE="${BUILD_CACHE_DIR}/gating-check-1.sig"
TMP_SIG="$(build_signature "gating-check-1" "$(cat "${TMP_CPP}")")"
if [[ ! -x "${TMP_BIN}" || ! -f "${TMP_SIG_FILE}" || "$(cat "${TMP_SIG_FILE}")" != "${TMP_SIG}" ]]; then
    rm -f "${TMP_BIN}"
    "${CXX}" -std=c++17 -O2 -Wall -Wextra -flto -I"${ROOT_DIR}/src" "${CXX_DEFS[@]}" "${TMP_CPP}" -o "${TMP_BIN}"
    printf '%s\n' "${TMP_SIG}" > "${TMP_SIG_FILE}"
fi
"${TMP_BIN}"

# --- TEST 5: Paired gating must undo cleanly and preserve the material key ---
cat > "${HARNESS_CPP}" <<'EOF'
#include <cassert>
#include <sstream>

#include "bitboard.h"
#include "endgame.h"
#include "piece.h"
#include "position.h"
#include "psqt.h"
#include "uci.h"
#include "variant.h"
#include "test_engine_init.hpp"

using namespace Stockfish;

static void load_variants() {
    std::istringstream ss(R"ini(
[symgating:chess]
gating = true
seirawanGating = true
symmetricDropTypes = r
)ini");
    variants.parse_istream<false>(ss);
}

int main() {
    init_test_engine();
    load_variants();

    StateInfo st{};
    Position pos;
    pos.set(variants.get("symgating"), "4k3/8/8/8/8/8/8/4K3[RR] w ABCDEFGH - 0 1", false, &st, nullptr);

    const Move m = make_gating<NORMAL>(SQ_E1, SQ_E2, ROOK, SQ_D1);
    assert(pos.legal(m));

    StateInfo next{};
    pos.do_move(m, next);
    assert(pos.pos_is_ok());

    pos.undo_move(m);
    assert(pos.pos_is_ok());
    return 0;
}
EOF

OBJ_FILES=()
while IFS= read -r -d '' obj; do
  OBJ_FILES+=("${obj}")
done < <(find "${ROOT_DIR}/src" -maxdepth 1 -name '*.o' ! -name 'main.o' -print0 | sort -z)

HARNESS_SIG_FILE="${BUILD_CACHE_DIR}/gating-check-roundtrip.sig"
HARNESS_SIG="$(build_signature "gating-check-roundtrip" "$(cat "${HARNESS_CPP}")")"
if [[ ! -x "${HARNESS_BIN}" || ! -f "${HARNESS_SIG_FILE}" || "$(cat "${HARNESS_SIG_FILE}")" != "${HARNESS_SIG}" ]]; then
    rm -f "${HARNESS_BIN}"
    (
      cd "${ROOT_DIR}/src"
      "${CXX}" -std=c++17 -O2 -Wall -Wextra -flto -I"${ROOT_DIR}/src" -I"${ROOT_DIR}/tests/lib" "${CXX_DEFS[@]}" "${HARNESS_CPP}" "${OBJ_FILES[@]}" -pthread -o "${HARNESS_BIN}"
    )
    printf '%s\n' "${HARNESS_SIG}" > "${HARNESS_SIG_FILE}"
fi
"${HARNESS_BIN}"

echo "gating-check-regression testing OK"
