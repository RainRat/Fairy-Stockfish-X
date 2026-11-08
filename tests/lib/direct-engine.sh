#!/usr/bin/env bash

fsx_check_direct_engine() {
  local root="$1" engine="$2" required_role="$3" profile board all
  [[ -x "$engine" ]] || { echo "required ${required_role} engine is missing: ${engine}" >&2; return 1; }
  [[ "${FSX_TEST_PROFILE:-strict}" == portable ]] && return 0
  source "${root}/tests/lib/build-signature.sh"
  profile=$(fsx_build_recorded_profile "$root" "$engine")
  board="${profile#*board=}"; board="${board%%;*}"
  all="${profile#*all=}"; all="${all%%;*}"
  case "${required_role}:${board}:${all}" in
    large-allvars:large:yes|large-allvars:very-large:yes|large-allvars:huge:yes) ;;
    *) echo "${required_role} engine required; ${engine} has profile ${profile:-unrecorded}" >&2; return 1 ;;
  esac
  if ! fsx_build_artifact_is_current "$root" "$engine" || find "${root}/src" -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.mk' -o -name 'Makefile' \) -newer "$engine" -print -quit | grep -q .; then
    echo "stale or unverified engine: ${engine}; rebuild it with tests/build.sh" >&2
    return 1
  fi
}
