#!/usr/bin/env bash

# Shared build/configuration helpers for C++ regression harnesses. Source this
# file after setting ROOT_DIR, ENGINE, CXX, and optionally JOBS.
#
# Named engine mappings:
#   stockfish       normal board, standard variants
#   stockfish-large large board, all variants
#   stockfish-allvars large board, all variants, NNUE-enabled executable
#   stockfish-vlb   very-large board, all variants, NNUE-enabled executable
# Unknown engine names reuse the existing position.o board-family probe and
# require their object family to have been prepared by the caller.

fsx_harness_hash_text() {
  local text="${1:-}"
  if command -v sha256sum >/dev/null 2>&1; then
    printf '%s' "${text}" | sha256sum | awk '{print $1}'
  elif command -v shasum >/dev/null 2>&1; then
    printf '%s' "${text}" | shasum -a 256 | awk '{print $1}'
  else
    printf '%s' "${text}" | wc -c | awk '{print $1}'
  fi
}

fsx_harness_hash_file() {
  local path="$1"
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "${path}" | awk '{print $1}'
  elif command -v shasum >/dev/null 2>&1; then
    shasum -a 256 "${path}" | awk '{print $1}'
  else
    stat -c '%Y %s' "${path}" 2>/dev/null || stat -f '%m %z' "${path}"
  fi
}

fsx_harness_makefile_hash() {
  fsx_harness_hash_file "${FSX_HARNESS_ROOT_DIR}/src/Makefile"
}

fsx_harness_compiler_signature() {
  "${FSX_HARNESS_CXX}" --version 2>&1 | sed -n '1p'
}

fsx_harness_helper_hash() {
  fsx_harness_hash_file "${BASH_SOURCE[0]}"
}

fsx_harness_source_tree_signature() {
  local path root
  local sig=""
  for root in "${FSX_HARNESS_ROOT_DIR}/src" \
              "${FSX_HARNESS_ROOT_DIR}/tests/lib" \
              "${FSX_HARNESS_ROOT_DIR}/tests/native"; do
    [[ -d "${root}" ]] || continue
    while IFS= read -r -d '' path; do
      sig+="$(fsx_harness_hash_file "${path}") ${path}"$'\n'
    done < <(find "${root}" -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) -print0 | sort -z)
  done
  fsx_harness_hash_text "${sig}"
}

fsx_harness_init() {
  local engine="$1"
  local root_dir="$2"
  local position_object
  local position_symbols

  FSX_HARNESS_ROOT_DIR="${root_dir}"
  FSX_HARNESS_ENGINE="${engine}"
  FSX_HARNESS_ENGINE_BASENAME=$(basename "${engine}")
  FSX_HARNESS_CXX="${CXX:-g++}"
  FSX_HARNESS_CXX_DEFS=(-DIS_64BIT -DUSE_PTHREADS)
  FSX_HARNESS_BUILD_ARGS=()
  FSX_HARNESS_BUILD_EXE=""
  FSX_HARNESS_KNOWN_ENGINE_CONFIG=false

  case "${FSX_HARNESS_ENGINE_BASENAME}" in
    stockfish)
      FSX_HARNESS_KNOWN_ENGINE_CONFIG=true
      FSX_HARNESS_BUILD_ARGS=(ARCH=x86-64 EXE=stockfish)
      FSX_HARNESS_BUILD_EXE=stockfish
      ;;
    stockfish-compound*|stockfish-arimaa*)
      FSX_HARNESS_KNOWN_ENGINE_CONFIG=true
      FSX_HARNESS_CXX_DEFS+=(-DLARGEBOARDS -DPRECOMPUTED_MAGICS -DALLVARS -DNNUE_EMBEDDING_OFF -DENABLE_COMPOUND_TURNS)
      FSX_HARNESS_BUILD_ARGS=(ARCH=x86-64 largeboards=yes all=yes nnue=yes EXE="${FSX_HARNESS_ENGINE_BASENAME}")
      FSX_HARNESS_BUILD_EXE="${FSX_HARNESS_ENGINE_BASENAME}"
      ;;
    stockfish-allvars*)
      FSX_HARNESS_KNOWN_ENGINE_CONFIG=true
      FSX_HARNESS_CXX_DEFS+=(-DLARGEBOARDS -DPRECOMPUTED_MAGICS -DALLVARS -DNNUE_EMBEDDING_OFF -DENABLE_COMPOUND_TURNS)
      FSX_HARNESS_BUILD_ARGS=(ARCH=x86-64 largeboards=yes all=yes nnue=yes EXE=stockfish-allvars)
      FSX_HARNESS_BUILD_EXE=stockfish-allvars
      ;;
    stockfish-large*)
      FSX_HARNESS_KNOWN_ENGINE_CONFIG=true
      FSX_HARNESS_CXX_DEFS+=(-DLARGEBOARDS -DPRECOMPUTED_MAGICS -DALLVARS -DNNUE_EMBEDDING_OFF -DENABLE_COMPOUND_TURNS)
      FSX_HARNESS_BUILD_ARGS=(ARCH=x86-64 largeboards=yes all=yes EXE=stockfish-large)
      FSX_HARNESS_BUILD_EXE=stockfish-large
      ;;
    stockfish-vlb*)
      FSX_HARNESS_KNOWN_ENGINE_CONFIG=true
      FSX_HARNESS_CXX_DEFS+=(-DLARGEBOARDS -DVERY_LARGE_BOARDS -DALLVARS -DNNUE_EMBEDDING_OFF -DENABLE_COMPOUND_TURNS)
      FSX_HARNESS_BUILD_ARGS=(ARCH=x86-64 largeboards=yes verylargeboards=yes all=yes nnue=yes EXE=stockfish-vlb)
      FSX_HARNESS_BUILD_EXE=stockfish-vlb
      ;;
  esac

  # Preserve support for custom engine names used by local harnesses. The
  # position object is the authority for the board macro family when no named
  # build target is available; VLB callers should use the named binary above.
  if [[ "${FSX_HARNESS_KNOWN_ENGINE_CONFIG}" == false ]]; then
    position_object="${FSX_HARNESS_ROOT_DIR}/src/position.o"
    if [[ -f "${position_object}" ]]; then
      position_symbols=$(nm -C "${position_object}" 2>/dev/null || true)
      if ! grep -F 'Position::fen(bool, bool, int, ' <<<"${position_symbols}" \
          | grep -F 'unsigned long) const' >/dev/null; then
        FSX_HARNESS_CXX_DEFS+=(-DLARGEBOARDS -DPRECOMPUTED_MAGICS -DALLVARS -DNNUE_EMBEDDING_OFF)
      fi
    fi
  fi
}

fsx_harness_prepare_objects() {
  local jobs="${1:-${JOBS:-2}}"

  if [[ "${FSX_REUSE_OBJECTS:-0}" == 1 || -z "${FSX_HARNESS_BUILD_EXE}" ]]; then
    return 0
  fi

  # Keep the caller's engine available while refreshing the object family.
  # objclean removes $(EXE), and the harness must not invalidate the engine it
  # is about to test.
  make -C "${FSX_HARNESS_ROOT_DIR}/src" EXE= objclean
  make -C "${FSX_HARNESS_ROOT_DIR}/src" -j"${jobs}" build "${FSX_HARNESS_BUILD_ARGS[@]}"

  if [[ ! -x "${FSX_HARNESS_ROOT_DIR}/src/${FSX_HARNESS_BUILD_EXE}" ]]; then
    echo "harness build did not produce ${FSX_HARNESS_BUILD_EXE}" >&2
    return 1
  fi
}

fsx_harness_prepare_objects_cached() {
  local cache_dir="$1"
  local label="${2:-harness objects}"
  local jobs="${3:-${JOBS:-2}}"
  local desired_signature object_signature

  # Custom engine names have no reliable Makefile configuration mapping, and
  # explicit reuse means the caller owns the object family. Do not certify
  # either set of objects as if this helper had prepared it.
  if [[ "${FSX_REUSE_OBJECTS:-0}" == 1 || -z "${FSX_HARNESS_BUILD_EXE}" ]]; then
    return 0
  fi

  mkdir -p "${cache_dir}"
  desired_signature=$(printf '%s|%s|%s|%s|%s|%s|%s|%s|%s\n' \
    "${FSX_HARNESS_ENGINE_BASENAME}" \
    "${FSX_HARNESS_CXX}" \
    "$(fsx_harness_compiler_signature)" \
    "${CXXFLAGS:-}" \
    "${FSX_HARNESS_CXX_DEFS[*]}" \
    "${FSX_HARNESS_BUILD_ARGS[*]}" \
    "$(fsx_harness_makefile_hash)" \
    "$(fsx_harness_helper_hash)" \
    "$(fsx_harness_source_tree_signature)")
  object_signature=$(fsx_harness_object_signature)

  if [[ -f "${cache_dir}/desired.sig" && -f "${cache_dir}/objects.sig" ]] \
      && [[ "$(<"${cache_dir}/desired.sig")" == "${desired_signature}" ]] \
      && [[ "$(<"${cache_dir}/objects.sig")" == "${object_signature}" ]] \
      && [[ -x "${FSX_HARNESS_ROOT_DIR}/src/${FSX_HARNESS_BUILD_EXE}" ]]; then
    echo "ok: ${label} (cached)"
    return 0
  fi

  fsx_harness_prepare_objects "${jobs}"
  object_signature=$(fsx_harness_object_signature)
  printf '%s\n' "${desired_signature}" > "${cache_dir}/desired.sig"
  printf '%s\n' "${object_signature}" > "${cache_dir}/objects.sig"
}

fsx_harness_collect_objects() {
  FSX_HARNESS_OBJ_FILES=()
  while IFS= read -r -d '' obj; do
    FSX_HARNESS_OBJ_FILES+=("${obj}")
  done < <(find "${FSX_HARNESS_ROOT_DIR}/src" -maxdepth 1 -name '*.o' ! -name 'main.o' -print0 | sort -z)

  if (( ${#FSX_HARNESS_OBJ_FILES[@]} == 0 )); then
    echo "no src/*.o objects found; build ${FSX_HARNESS_ENGINE} before running this test" >&2
    return 1
  fi
}

fsx_harness_object_signature() {
  local obj
  local sig=""
  while IFS= read -r -d '' obj; do
    sig+="${obj##*/} $(fsx_harness_hash_file "${obj}")"$'\n'
  done < <(find "${FSX_HARNESS_ROOT_DIR}/src" -maxdepth 1 -name '*.o' ! -name 'main.o' -print0 | sort -z)
  fsx_harness_hash_text "${sig}"
}

fsx_harness_engine_signature() {
  if [[ -e "${FSX_HARNESS_ENGINE}" ]]; then
    fsx_harness_hash_file "${FSX_HARNESS_ENGINE}"
  else
    echo "missing"
  fi
}

fsx_harness_signature() {
  local label="$1"
  local source_file="$2"
  local source_signature="missing"
  [[ -f "${source_file}" ]] && source_signature=$(fsx_harness_hash_file "${source_file}")

  printf '%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s\n' \
    "${label}" \
    "${FSX_HARNESS_CXX}" \
    "$(fsx_harness_compiler_signature)" \
    "${CXXFLAGS:-}" \
    "${FSX_HARNESS_ENGINE_BASENAME}" \
    "${FSX_HARNESS_CXX_DEFS[*]}" \
    "$(fsx_harness_makefile_hash)" \
    "$(fsx_harness_helper_hash)" \
    "$(fsx_harness_engine_signature)" \
    "$(fsx_harness_object_signature)" \
    "${source_signature}"
}

fsx_harness_build() {
  local source_file="$1"
  local output_file="$2"
  local label="$3"
  local signature_file="${4:-${output_file}.sig}"
  local signature
  local extra_cxxflags=()

  read -r -a extra_cxxflags <<<"${CXXFLAGS:-}"

  signature=$(fsx_harness_signature "${label}" "${source_file}")
  if [[ -x "${output_file}" && -f "${signature_file}" \
      && "$(<"${signature_file}")" == "${signature}" ]]; then
    return 0
  fi

  rm -f "${output_file}"
  (
    cd "${FSX_HARNESS_ROOT_DIR}/src"
    "${FSX_HARNESS_CXX}" "${extra_cxxflags[@]}" -std=c++17 -O2 -Wall -Wextra -flto \
      -I"${FSX_HARNESS_ROOT_DIR}/src" -I"${FSX_HARNESS_ROOT_DIR}/tests/lib" \
      "${FSX_HARNESS_CXX_DEFS[@]}" "${source_file}" \
      "${FSX_HARNESS_OBJ_FILES[@]}" -pthread -o "${output_file}"
  )
  printf '%s\n' "${signature}" > "${signature_file}"
}
