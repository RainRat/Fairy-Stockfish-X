#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
mkdir -p "${ROOT_DIR}/.local/build"
OUT_DIR=$(mktemp -d "${ROOT_DIR}/.local/build/build-failure-paths.XXXXXX")
trap 'rm -rf "${OUT_DIR}"' EXIT
mkdir -p "${OUT_DIR}/src" "${OUT_DIR}/tools" "${OUT_DIR}/no-hash"
cp "${ROOT_DIR}/src/Makefile" "${ROOT_DIR}/src/board-size.mk" "${OUT_DIR}/src/"
printf '#define EvalFileDefaultName "nn-0123456789ab.nnue"\n' >"${OUT_DIR}/src/evaluate.h"
printf '\n' >"${OUT_DIR}/src/.depend"
MAKE_BIN=$(command -v make)

cat >"${OUT_DIR}/fail-cxx" <<'SH'
#!/usr/bin/env bash
printf 'injected compiler failure\n' >&2
exit 41
SH
chmod +x "${OUT_DIR}/fail-cxx"

# A failed dependency scan must leave the previous dependency file untouched.
printf 'previous.o: previous.cpp\n' >"${OUT_DIR}/src/.depend"
if make -B -C "${OUT_DIR}/src" -f Makefile .depend CXX="${OUT_DIR}/fail-cxx" \
    >"${OUT_DIR}/depend.log" 2>&1; then
  echo 'dependency generation unexpectedly succeeded' >&2
  exit 1
fi
grep -qx 'previous.o: previous.cpp' "${OUT_DIR}/src/.depend"
if compgen -G "${OUT_DIR}/src/.depend.tmp.*" >/dev/null; then echo 'dependency failure left a temporary file' >&2; exit 1; fi

# A failed link must preserve the prior executable and remove the link temporary.
printf 'object\n' >"${OUT_DIR}/src/object.o"
printf 'previous executable contents\n' >"${OUT_DIR}/src/probe"
if ! make -B -C "${OUT_DIR}/src" -f Makefile -o .depend EXE=probe OBJS=object.o \
  LDFLAGS= CXX="${OUT_DIR}/fail-cxx" objects-clean >"${OUT_DIR}/objects-clean.log" 2>&1; then
  cat "${OUT_DIR}/objects-clean.log" >&2
  exit 1
fi
[[ -f "${OUT_DIR}/src/probe" ]]
[[ ! -e "${OUT_DIR}/src/.depend" ]]
printf 'object\n' >"${OUT_DIR}/src/object.o"
printf 'previous executable contents\n' >"${OUT_DIR}/src/probe"
touch -d '2000-01-01' "${OUT_DIR}/src/object.o"
if make -B -C "${OUT_DIR}/src" -f Makefile -o .depend EXE=probe OBJS=object.o \
    LDFLAGS= CXX="${OUT_DIR}/fail-cxx" probe >"${OUT_DIR}/link.log" 2>&1; then
  echo 'injected link unexpectedly succeeded' >&2
  exit 1
fi
grep -qx 'previous executable contents' "${OUT_DIR}/src/probe"
if compgen -G "${OUT_DIR}/src/probe.tmp.*" >/dev/null; then
  echo 'failed link left a temporary executable' >&2
  exit 1
fi

# The NNUE target must fail if checksum tools are unavailable, and failed
# downloads must not replace an existing network file.
for command in uname grep sed which cut expr g++; do
  path=$(command -v "$command")
  ln -sf "$path" "${OUT_DIR}/no-hash/$command"
done
for command in curl wget; do
  cat >"${OUT_DIR}/tools/$command" <<'SH'
#!/usr/bin/env sh
if [ "${FSX_MOCK_DOWNLOAD:-fail}" = corrupt ]; then
  output=
  while [ "$#" -gt 0 ]; do
    if [ "$1" = -o ] || [ "$1" = -O ]; then shift; output=$1; fi
    shift
done
  printf 'wrong network contents\n' >"$output"
  exit 0
fi
exit 22
SH
  chmod +x "${OUT_DIR}/tools/$command"
done
ln -sf "${OUT_DIR}/tools/curl" "${OUT_DIR}/no-hash/curl"
ln -sf "${OUT_DIR}/tools/wget" "${OUT_DIR}/no-hash/wget"
for command in shasum sha256sum; do
  [[ ! -e "${OUT_DIR}/no-hash/$command" ]]
done
if PATH="${OUT_DIR}/no-hash" "${MAKE_BIN}" -C "${OUT_DIR}/src" net \
    >"${OUT_DIR}/no-hash.log" 2>&1; then
  echo 'NNUE target unexpectedly accepted missing checksum tools' >&2
  exit 1
fi
grep -q 'sha256sum or shasum is required' "${OUT_DIR}/no-hash.log"

# With verification available, both failed download sources must fail the
# target and preserve any pre-existing network file.
net=nn-0123456789ab.nnue
printf 'previous network contents\n' >"${OUT_DIR}/src/$net"
if PATH="${OUT_DIR}/tools:$PATH" "${MAKE_BIN}" -C "${OUT_DIR}/src" net \
    >"${OUT_DIR}/download.log" 2>&1; then
  echo 'NNUE target unexpectedly accepted failed downloads' >&2
  exit 1
fi
grep -qx 'previous network contents' "${OUT_DIR}/src/$net"
if compgen -G "${OUT_DIR}/src/$net.tmp.*" >/dev/null; then echo 'NNUE failure left a temporary download' >&2; exit 1; fi
grep -q 'Failed to download and validate' "${OUT_DIR}/download.log"
if FSX_MOCK_DOWNLOAD=corrupt PATH="${OUT_DIR}/tools:$PATH" "${MAKE_BIN}" \
    -C "${OUT_DIR}/src" net >"${OUT_DIR}/checksum.log" 2>&1; then
  echo 'NNUE target unexpectedly accepted a checksum mismatch' >&2
  exit 1
fi
grep -qx 'previous network contents' "${OUT_DIR}/src/$net"
grep -q 'checksum mismatch' "${OUT_DIR}/checksum.log"
if compgen -G "${OUT_DIR}/src/$net.tmp.*" >/dev/null; then echo 'checksum failure left a temporary download' >&2; exit 1; fi

echo 'build failure paths preserve dependencies and executables and fail on unverified NNUE downloads'
