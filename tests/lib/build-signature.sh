#!/usr/bin/env bash

# Build configuration and artifact fingerprints shared by build/test wrappers.

fsx_build_hash_text() {
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum | awk '{print $1}'
  elif command -v shasum >/dev/null 2>&1; then
    shasum -a 256 | awk '{print $1}'
  else
    cksum | awk '{print $1}'
  fi
}

fsx_build_hash_file() {
  fsx_build_hash_text <"$1"
}

fsx_build_makefile_signature() {
  {
    fsx_build_hash_file "$1/src/Makefile"
    fsx_build_hash_file "$1/src/board-size.mk"
  } | fsx_build_hash_text
}

fsx_board_family() {
  case "$1" in
    64|normal) echo normal ;;
    128|large) echo large ;;
    256|very-large) echo very-large ;;
    512|huge) echo huge ;;
    *) echo "unknown board family: $1" >&2; return 1 ;;
  esac
}

fsx_board_bits() {
  case "$(fsx_board_family "$1")" in
    normal) echo 64 ;; large) echo 128 ;; very-large) echo 256 ;; huge) echo 512 ;;
    *) return 1 ;;
  esac
}

# Zero-based maximum file/rank indices.
fsx_board_limits() {
  case "$(fsx_board_bits "$1")" in
    64) echo '7 7' ;; 128) echo '11 9' ;; 256) echo '15 15' ;; 512) echo '21 21' ;;
    *) return 1 ;;
  esac
}

fsx_profile_board_family() {
  local board="${1#*board=}"
  fsx_board_family "${board%%;*}"
}

fsx_engine_family() {
  local root_dir="$1" engine="$2" profile
  if [[ -n "${FSX_ENGINE_FAMILY:-}" ]]; then
    fsx_board_family "$FSX_ENGINE_FAMILY"
    return
  fi
  profile=$(fsx_build_recorded_profile "$root_dir" "$engine" 2>/dev/null) || profile=""
  if [[ -n "$profile" ]]; then
    fsx_profile_board_family "$profile"
    return
  fi
  case "${engine##*/}" in
    stockfish-512*|stockfish-huge*) echo huge ;;
    stockfish-vlb*) echo very-large ;;
    stockfish-large*|stockfish-allvars*) echo large ;;
    *) echo normal ;;
  esac
}

fsx_build_output_path() {
  local root_dir="$1"
  local exe="$2"
  case "$exe" in
    /*) printf '%s\n' "$exe" ;;
    *) printf '%s/src/%s\n' "$root_dir" "$exe" ;;
  esac
}

fsx_build_signature_file() {
  local root_dir="$1"
  local output_file="$2"
  local output_key
  if [[ "$output_file" != /* && -f "$output_file" ]]; then
    output_file="$(cd "$(dirname "$output_file")" && pwd)/${output_file##*/}"
  fi
  output_key=$(printf '%s' "$output_file" | fsx_build_hash_text)
  printf '%s/.local/build/signatures/%s.sig\n' "$root_dir" "$output_key"
}

fsx_build_compiler_version() {
  local compiler="${CXX:-g++}"
  local version
  if version=$("$compiler" --version 2>&1 | sed -n '1p'); then
    printf '%s\n' "$version"
  else
    printf 'unavailable: %s\n' "$compiler"
  fi
}

fsx_build_signature() {
  local root_dir="$1"
  local output_file="$2"
  shift 2

  {
    printf 'output=%s\n' "$output_file"
    printf 'compiler=%s\n' "${CXX:-g++}"
    printf 'compiler-version=%s\n' "$(fsx_build_compiler_version)"
    printf 'CXXFLAGS=%s\n' "${CXXFLAGS:-}"
    printf 'EXTRACXXFLAGS=%s\n' "${EXTRACXXFLAGS:-}"
    printf 'LDFLAGS=%s\n' "${LDFLAGS:-}"
    printf 'makefiles=%s\n' "$(fsx_build_makefile_signature "$root_dir")"
    printf 'arguments:\n'
    printf '%s\n' "$@"
  } | fsx_build_hash_text
}

fsx_build_profile() {
  local arch=x86-64-modern board=normal largeboards=no verylargeboards=no
  local all=no nnue=no debug=no optimize=yes
  local hugeboards=no
  local compiler="${CXX:-g++}" compiler_kind="${COMP:-native}" arg

  for arg in "$@"; do
    case "$arg" in
      ARCH=*) arch="${arg#ARCH=}" ;;
      largeboards=*) largeboards="${arg#largeboards=}" ;;
      verylargeboards=*) verylargeboards="${arg#verylargeboards=}" ;;
      hugeboards=*) hugeboards="${arg#hugeboards=}" ;;
      all=*) all="${arg#all=}" ;;
      nnue=*) nnue="${arg#nnue=}" ;;
      debug=*) debug="${arg#debug=}" ;;
      optimize=*) optimize="${arg#optimize=}" ;;
      COMP=*) compiler_kind="${arg#COMP=}" ;;
    esac
  done

  if [[ "$hugeboards" == yes ]]; then
    board=huge
  elif [[ "$verylargeboards" == yes ]]; then
    board=very-large
  elif [[ "$largeboards" == yes ]]; then
    board=large
  fi

  printf 'arch=%s;board=%s;all=%s;nnue=%s;debug=%s;optimize=%s;compiler=%s;comp=%s\n' \
    "$arch" "$board" "$all" "$nnue" "$debug" "$optimize" "$compiler" "$compiler_kind"
}

fsx_build_config_file() {
  printf '%s/src/.build-config\n' "$1"
}

# Named executables share src/*.o. Track the object configuration independently
# of the output name, retaining every other argument and environment flag.
fsx_build_object_config() {
  local root_dir="$1" arg
  shift
  local -a object_args=()
  for arg in "$@"; do
    case "$arg" in
      EXE=*) ;;
      *) object_args+=("$arg") ;;
    esac
  done
  fsx_build_signature "$root_dir" shared-objects "${object_args[@]}"
}

fsx_build_object_config_matches() {
  local root_dir="$1" config_file expected
  shift
  config_file=$(fsx_build_config_file "$root_dir")
  expected=$(fsx_build_object_config "$root_dir" "$@")
  [[ -f "$config_file" && "$(cat "$config_file")" == "$expected" ]]
}

fsx_build_write_object_config() {
  local root_dir="$1" config_file temp_file
  shift
  config_file=$(fsx_build_config_file "$root_dir")
  temp_file="${config_file}.tmp.$$"
  fsx_build_object_config "$root_dir" "$@" >"$temp_file"
  mv -f "$temp_file" "$config_file"
}

fsx_build_signature_matches() {
  local root_dir="$1"
  local output_file="$2"
  local expected_signature="$3"
  local expected_profile="${4:-}" signature_file recorded_engine_hash

  signature_file=$(fsx_build_signature_file "$root_dir" "$output_file")
  [[ -x "$output_file" && -f "$signature_file" ]] || return 1
  [[ "$(sed -n '1p' "$signature_file")" == "$expected_signature" ]] || return 1
  if [[ -n "$expected_profile" ]]; then
    [[ "$(sed -n '3s/^profile=//p' "$signature_file")" == "$expected_profile" ]] || return 1
  fi
  recorded_engine_hash=$(sed -n '2p' "$signature_file")
  [[ -n "$recorded_engine_hash" ]] || return 1
  [[ "$recorded_engine_hash" == "$(fsx_build_hash_file "$output_file")" ]]
}

fsx_build_write_signature() {
  local root_dir="$1"
  local output_file="$2"
  local build_signature="$3"
  local build_profile="${4:-unknown}" signature_file temp_file

  signature_file=$(fsx_build_signature_file "$root_dir" "$output_file")
  mkdir -p "$(dirname "$signature_file")"
  temp_file="${signature_file}.tmp.$$"
  printf '%s\n%s\nprofile=%s\n' "$build_signature" "$(fsx_build_hash_file "$output_file")" "$build_profile" >"$temp_file"
  mv -f "$temp_file" "$signature_file"
}

fsx_build_recorded_profile() {
  local root_dir="$1"
  local output_file="$2"
  local signature_file

  signature_file=$(fsx_build_signature_file "$root_dir" "$output_file")
  sed -n '3s/^profile=//p' "$signature_file"
}

fsx_build_artifact_is_current() {
  local root_dir="$1"
  local output_file="$2"
  local signature_file recorded_engine_hash

  signature_file=$(fsx_build_signature_file "$root_dir" "$output_file")
  [[ -x "$output_file" && -f "$signature_file" ]] || return 1
  recorded_engine_hash=$(sed -n '2p' "$signature_file")
  [[ -n "$recorded_engine_hash" ]] || return 1
  [[ "$recorded_engine_hash" == "$(fsx_build_hash_file "$output_file")" ]] || return 1
  ! find "${root_dir}/src" -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.mk' -o -name 'Makefile' \) \
    -newer "$signature_file" -print -quit | grep -q .
}
