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
    printf 'makefile=%s\n' "$(fsx_build_hash_file "${root_dir}/src/Makefile")"
    printf 'arguments:\n'
    printf '%s\n' "$@"
  } | fsx_build_hash_text
}

fsx_build_profile() {
  local arch=x86-64-modern board=normal largeboards=no verylargeboards=no
  local all=no arimaa=no nnue=no debug=no optimize=yes
  local compiler="${CXX:-g++}" compiler_kind="${COMP:-native}" arg

  for arg in "$@"; do
    case "$arg" in
      ARCH=*) arch="${arg#ARCH=}" ;;
      largeboards=*) largeboards="${arg#largeboards=}" ;;
      verylargeboards=*) verylargeboards="${arg#verylargeboards=}" ;;
      all=*) all="${arg#all=}" ;;
      arimaa=*) arimaa="${arg#arimaa=}" ;;
      nnue=*) nnue="${arg#nnue=}" ;;
      debug=*) debug="${arg#debug=}" ;;
      optimize=*) optimize="${arg#optimize=}" ;;
      COMP=*) compiler_kind="${arg#COMP=}" ;;
    esac
  done

  if [[ "$verylargeboards" == yes ]]; then
    board=very-large
  elif [[ "$largeboards" == yes ]]; then
    board=large
  fi

  printf 'arch=%s;board=%s;all=%s;arimaa=%s;nnue=%s;debug=%s;optimize=%s;compiler=%s;comp=%s\n' \
    "$arch" "$board" "$all" "$arimaa" "$nnue" "$debug" "$optimize" "$compiler" "$compiler_kind"
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
  ! find "${root_dir}/src" -type f \( -name '*.cpp' -o -name '*.h' -o -name 'Makefile' \) \
    -newer "$signature_file" -print -quit | grep -q .
}
