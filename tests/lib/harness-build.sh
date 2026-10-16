#!/usr/bin/env bash

# Shared build/configuration helpers for C++ regression harnesses. Source this
# file after setting ROOT_DIR, ENGINE, CXX, and optionally JOBS.
#
# Harness object mappings (board family for compiled test objects; the
# engine binary profiles themselves are verified by tests/run.sh and
# tests/regression-runner.sh, e.g. stockfish-large is board=large/all=no and
# stockfish-allvars is board=large/all=yes/nnue=no while the harnesses below
# build objects with all=yes and, for allvars/vlb, nnue=yes for NNUE paths):
#   stockfish       normal board, standard variants
#   stockfish-large large board, all variants (objects)
#   stockfish-allvars large board, all variants, NNUE-enabled objects
#   stockfish-vlb   very-large board, all variants, NNUE-enabled objects
# Recorded profiles also support custom engine names and carry their
# ARCH/debug/optimize flags into the harness objects. Unrecorded custom names
# reuse the existing position.o probe; their objects are owned by the caller.

_HARNESS_LIB_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${_HARNESS_LIB_DIR}/build-signature.sh"

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
  fsx_build_makefile_signature "${FSX_HARNESS_ROOT_DIR}"
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
  local engine="$1" root_dir="$2" family bits
  local recorded_profile field arch=x86-64 all=no nnue=no
  local debug="" optimize="" position_symbols

  FSX_HARNESS_ROOT_DIR="$root_dir"
  FSX_HARNESS_ENGINE="$engine"
  FSX_HARNESS_ENGINE_BASENAME=${engine##*/}
  FSX_HARNESS_CXX="${CXX:-g++}"
  FSX_HARNESS_CXX_DEFS=(-DIS_64BIT -DUSE_PTHREADS -DNNUE_EMBEDDING_OFF)
  FSX_HARNESS_BUILD_ARGS=()
  FSX_HARNESS_BUILD_EXE=""
  FSX_HARNESS_KNOWN_ENGINE_CONFIG=false

  # Named harness roles exercise all variants and NNUE paths where required.
  case "$FSX_HARNESS_ENGINE_BASENAME" in
    stockfish) FSX_HARNESS_BUILD_EXE=stockfish ;;
    stockfish-allvars*) FSX_HARNESS_BUILD_EXE=stockfish-allvars; all=yes; nnue=yes ;;
    stockfish-large*) FSX_HARNESS_BUILD_EXE=stockfish-large; all=yes ;;
    stockfish-vlb*) FSX_HARNESS_BUILD_EXE=stockfish-vlb; all=yes; nnue=yes ;;
    stockfish-512*|stockfish-huge*) FSX_HARNESS_BUILD_EXE=${FSX_HARNESS_ENGINE_BASENAME%%-nnue*}; all=yes ;;
  esac

  recorded_profile=$(fsx_build_recorded_profile "$root_dir" "$engine" 2>/dev/null || true)
  local -a fields=()
  IFS=';' read -r -a fields <<<"$recorded_profile"
  for field in "${fields[@]}"; do
    case "$field" in
      arch=*) arch=${field#arch=} ;;
      debug=yes|debug=no) debug=${field#debug=} ;;
      optimize=yes|optimize=no) optimize=${field#optimize=} ;;
      all=yes|all=no) all=${field#all=} ;;
      nnue=yes|nnue=no) nnue=${field#nnue=} ;;
    esac
  done

  if [[ -n "$recorded_profile" || -n "$FSX_HARNESS_BUILD_EXE" ]]; then
    # Object ABI follows the actual build, irrespective of suite family hints.
    family=$(FSX_ENGINE_FAMILY= fsx_engine_family "$root_dir" "$engine")
    FSX_HARNESS_KNOWN_ENGINE_CONFIG=true
    [[ -n "$FSX_HARNESS_BUILD_EXE" ]] || FSX_HARNESS_BUILD_EXE="$FSX_HARNESS_ENGINE_BASENAME"
  else
    family=$(fsx_engine_family "$root_dir" "$engine")
    # Preserve the legacy 64/128-bit object probe for unrecorded custom builds.
    if [[ -z "${FSX_ENGINE_FAMILY:-}" && -f "$root_dir/src/position.o" ]]; then
      position_symbols=$(nm -C "$root_dir/src/position.o" 2>/dev/null || true)
      if ! grep -F 'Position::fen(bool, bool, int, ' <<<"$position_symbols" \
          | grep -F 'unsigned long) const' >/dev/null; then
        family=large
        all=yes
      fi
    fi
    [[ "$family" == normal ]] || all=yes
  fi
  bits=$(fsx_board_bits "$family")
  FSX_HARNESS_CXX_DEFS+=("-DBOARD_BITS=$bits")
  [[ "$bits" != 128 ]] || FSX_HARNESS_CXX_DEFS+=(-DPRECOMPUTED_MAGICS)
  [[ "$all" != yes ]] || FSX_HARNESS_CXX_DEFS+=(-DALLVARS)
  if [[ "$FSX_HARNESS_KNOWN_ENGINE_CONFIG" == true ]]; then
    case "$family" in
      huge) FSX_HARNESS_BUILD_ARGS=("ARCH=$arch" hugeboards=yes "all=$all" "nnue=$nnue" "EXE=$FSX_HARNESS_BUILD_EXE") ;;
      very-large) FSX_HARNESS_BUILD_ARGS=("ARCH=$arch" verylargeboards=yes "all=$all" "nnue=$nnue" "EXE=$FSX_HARNESS_BUILD_EXE") ;;
      large) FSX_HARNESS_BUILD_ARGS=("ARCH=$arch" largeboards=yes "all=$all" "nnue=$nnue" "EXE=$FSX_HARNESS_BUILD_EXE") ;;
      *) FSX_HARNESS_BUILD_ARGS=("ARCH=$arch" largeboards=no "all=$all" "nnue=$nnue" "EXE=$FSX_HARNESS_BUILD_EXE") ;;
    esac
    [[ -z "$debug" ]] || FSX_HARNESS_BUILD_ARGS+=("debug=$debug")
    [[ -z "$optimize" ]] || FSX_HARNESS_BUILD_ARGS+=("optimize=$optimize")
  fi
}

fsx_harness_prepare_objects() {
  local jobs="${1:-${JOBS:-$(nproc 2>/dev/null || echo 2)}}"

  if [[ "${FSX_REUSE_OBJECTS:-0}" == 1 || -z "${FSX_HARNESS_BUILD_EXE}" ]]; then
    return 0
  fi

  # Refresh harness objects without deleting or relinking the tested engine.
  # Its wrapper-recorded profile and binary hash must remain valid.
  # Drop the generated dependency file too: it can reference headers removed
  # by refactors, which breaks the build outright until regenerated.
  rm -f "${FSX_HARNESS_ROOT_DIR}/src/.depend" "$(fsx_build_config_file "${FSX_HARNESS_ROOT_DIR}")"
  make -C "${FSX_HARNESS_ROOT_DIR}/src" -s EXE= objclean || return 1
  make -C "${FSX_HARNESS_ROOT_DIR}/src" -s -j"${jobs}" build-objects "${FSX_HARNESS_BUILD_ARGS[@]}" || return 1
  fsx_build_write_object_config "${FSX_HARNESS_ROOT_DIR}" "${FSX_HARNESS_BUILD_ARGS[@]}"
}

fsx_harness_prepare_objects_cached() {
  local cache_dir="$1"
  local label="${2:-harness objects}"
  local jobs="${3:-${JOBS:-$(nproc 2>/dev/null || echo 2)}}"
  local desired_signature object_signature

  # Unrecorded custom engines and explicit reuse leave objects to the caller.
  # Do not certify
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
      && [[ -x "${FSX_HARNESS_ENGINE}" ]]; then
    echo "ok: ${label} (cached)"
    return 0
  fi

  fsx_harness_prepare_objects "${jobs}" || return 1
  object_signature=$(fsx_harness_object_signature)
  local tmp_desired="${cache_dir}/desired.sig.tmp.$$"
  local tmp_objects="${cache_dir}/objects.sig.tmp.$$"
  printf '%s\n' "${desired_signature}" > "${tmp_desired}"
  printf '%s\n' "${object_signature}" > "${tmp_objects}"
  mv -f "${tmp_desired}" "${cache_dir}/desired.sig"
  mv -f "${tmp_objects}" "${cache_dir}/objects.sig"
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
  if (($# > 3)); then
    shift 4
  else
    shift "$#"
  fi
  local -a extra_sources=("$@")
  local signature
  local extra_cxxflags=()

  read -r -a extra_cxxflags <<<"${CXXFLAGS:-}"

  signature=$(fsx_harness_signature "${label}" "${source_file}")
  local extra_source
  for extra_source in "${extra_sources[@]}"; do
    signature+="|${extra_source}=$(fsx_harness_hash_file "${extra_source}")"
  done
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
      "${extra_sources[@]}" "${FSX_HARNESS_OBJ_FILES[@]}" -pthread -o "${output_file}"
  )
  printf '%s\n' "${signature}" > "${signature_file}"
}
