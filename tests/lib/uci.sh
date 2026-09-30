#!/bin/bash

# Detect project root directory relative to this script
UCI_LIB_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ROOT_DIR=$(cd "${UCI_LIB_DIR}/../.." && pwd)

fsx_error() {
  local test_name="$1"
  local line="$2"
  echo "${test_name} failed on line ${line}" >&2
  exit 1
}

FSX_EXIT_CLEANUPS=()

fsx_run_exit_cleanups() {
  local status=$?
  local cleanup

  set +e
  for cleanup in "${FSX_EXIT_CLEANUPS[@]}"; do
    eval "${cleanup}"
  done

  return "${status}"
}

fsx_add_exit_cleanup() {
  local cleanup="${1:-}"

  if [[ -z "${cleanup}" ]]; then
    return
  fi

  if [[ ${#FSX_EXIT_CLEANUPS[@]} -eq 0 ]]; then
    trap fsx_run_exit_cleanups EXIT
  fi

  FSX_EXIT_CLEANUPS+=("${cleanup}")
}

setup_test_context() {
  local engine_arg="${1:-}"
  local variants_arg="${2:-}"
  local test_name="${3:-${BASH_SOURCE[1]##*/}}"

  if [[ -z "${SCRIPT_DIR:-}" ]]; then
    SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[1]}")" && pwd)
  fi
  if [[ -z "${ROOT_DIR:-}" ]]; then
    ROOT_DIR=$(cd "${SCRIPT_DIR}/.." && pwd)
  fi

  ENGINE="${ENGINE:-$(default_engine "${engine_arg}")}"
  VARIANTS="${VARIANTS:-$(default_variants "${variants_arg}")}"
  VARIANT_PATH="${VARIANTS}"
  export SCRIPT_DIR ROOT_DIR ENGINE VARIANTS VARIANT_PATH

  FSX_TEST_NAME="${test_name}"
  export FSX_TEST_NAME
  set -E
  trap 'fsx_error "${FSX_TEST_NAME}" "${LINENO}"' ERR
}

init_test_env() {
  setup_test_context "$@"
}

default_engine() {
  local custom_engine="${1:-}"
  if [[ -n "$custom_engine" ]]; then
    case "$custom_engine" in
      /*) echo "$custom_engine" ;;
      *) echo "${ROOT_DIR}/${custom_engine}" ;;
    esac
  elif [[ -x "${ROOT_DIR}/src/stockfish" ]]; then
    echo "${ROOT_DIR}/src/stockfish"
  else
    echo "${ROOT_DIR}/stockfish"
  fi
}

default_variants() {
  local custom_variants="${1:-}"
  if [[ -n "$custom_variants" ]]; then
    case "$custom_variants" in
      /*) echo "$custom_variants" ;;
      *) echo "${ROOT_DIR}/${custom_variants}" ;;
    esac
  else
    echo "${ROOT_DIR}/src/variants.ini"
  fi
}

assert_contains_literal() {
  local haystack="$1"
  local needle="$2"
  local context="${3:-contains}"

  if ! grep -Fq "$needle" <<<"$haystack"; then
    echo "expected output to ${context}: $needle" >&2
    echo "actual output:" >&2
    printf '%s\n' "$haystack" >&2
    return 1
  fi
}

assert_not_contains_literal() {
  local haystack="$1"
  local needle="$2"
  local context="${3:-not contain}"

  if grep -Fq "$needle" <<<"$haystack"; then
    echo "expected output to ${context}: $needle" >&2
    echo "actual output:" >&2
    printf '%s\n' "$haystack" >&2
    return 1
  fi
}

uci_timeout() {
  timeout "${UCI_TIMEOUT:-60s}" "$@"
}

run_uci() {
  local engine="$1"
  local variant_path="$2"
  local variant="$3"
  shift 3

  {
    printf 'uci\n'
    printf 'setoption name VariantPath value %s\n' "$variant_path"
    printf 'setoption name UCI_Variant value %s\n' "$variant"
    cat
    printf 'quit\n'
  } | if [[ "${FSX_SHOW_VARIANT_LOAD_SUMMARIES:-0}" == 1 \
          || "${FSX_QUIET_VARIANT_LOAD_SUMMARIES:-0}" != 1 ]]; then
    uci_timeout "$engine"
  else
    uci_timeout "$engine" 2> >(sed \
      -e '/^\[[0-9][0-9]*\] variants skipped because of board size limits/d' \
      -e '/^\[[0-9][0-9]*\] variant templates not found or skipped because of board size limits/d' >&2)
  fi
}

run_uci_cmds() {
  local engine="$1"
  local variant_path="$2"
  local variant="$3"
  local cmds="$4"
  run_uci "$engine" "$variant_path" "$variant" <<< "$cmds"
}

run_cmds() {
  run_uci_cmds "$@"
}

declare -A FSX_VARIANT_CATALOG_LOADED=()
declare -A FSX_VARIANT_CATALOG_OUTPUT=()

fsx_variant_catalog() {
  local engine="$1"
  local variant_path="${2:-${VARIANTS}}"
  local cache_key="${engine}"$'\x1f'"${variant_path}"

  if [[ -z "${FSX_VARIANT_CATALOG_LOADED["$cache_key"]:-}" ]]; then
    FSX_VARIANT_CATALOG_OUTPUT["$cache_key"]=$( {
      printf 'uci\n'
      printf 'setoption name VariantPath value %s\n' "$variant_path"
      printf 'uci\n'
      printf 'quit\n'
    } | uci_timeout "$engine" 2>&1 )
    FSX_VARIANT_CATALOG_LOADED["$cache_key"]=1
  fi

  printf '%s' "${FSX_VARIANT_CATALOG_OUTPUT["$cache_key"]}"
}

# True when the engine's UCI_Variant combo lists the variant. Backs probes
# for variants that print no selection banner (standard variants).
fsx_variant_listed_in_catalog() {
  local engine="$1"
  local variant="$2"
  local variant_path="${3:-${VARIANTS}}"
  local catalog

  catalog=$(fsx_variant_catalog "$engine" "$variant_path")
  awk -v target="$variant" '
    $1 == "option" {
      for (i = 1; i < NF; ++i)
        if ($i == "var" && $(i + 1) == target)
          found = 1
    }
    END { exit !found }
  ' <<<"$catalog"
}

probe_variant_available() {
  local engine="$1"
  local variant="$2"
  local variant_path="${3:-${VARIANTS}}"
  local out status=1

  fsx_variant_load_metadata "$variant_path"
  local metadata_key="${variant_path}::${variant}"
  if [[ -n "${FSX_VARIANT_PARENT["$metadata_key"]+declared}" ]]; then
    out=$(fsx_variant_catalog "$engine" "$variant_path")
    if fsx_variant_listed_in_catalog "$engine" "$variant" "$variant_path"; then
      status=0
    fi
  else
    out=$(FSX_SHOW_VARIANT_LOAD_SUMMARIES=1 run_uci "$engine" "$variant_path" "$variant" <<<'d' 2>&1)
    if grep -Fq "info string variant ${variant} " <<<"$out"; then
      status=0
    elif fsx_variant_listed_in_catalog "$engine" "$variant" "$variant_path"; then
      # Standard variants intentionally print no info line on selection;
      # catalog membership proves the engine registered the variant.
      status=0
    fi
  fi
  FSX_VARIANT_PROBE_OUTPUT="$out"
  export FSX_VARIANT_PROBE_OUTPUT
  return "$status"
}

declare -A FSX_VARIANT_META_LOADED=()
declare -A FSX_VARIANT_PARENT=()
declare -A FSX_VARIANT_MAX_FILE=()
declare -A FSX_VARIANT_MAX_RANK=()

fsx_file_index() {
  local value="${1,,}"
  if [[ "$value" =~ ^[0-9]+$ ]]; then
    echo $((10#$value - 1))
  else
    local code
    printf -v code '%d' "'${value:0:1}"
    echo $((code - 97))
  fi
}

fsx_variant_load_metadata() {
  local variant_path="$1"
  local current_variant=""
  local current_parent=""
  local line section

  if [[ -n "${FSX_VARIANT_META_LOADED["$variant_path"]:-}" ]]; then
    return
  fi

  while IFS= read -r line || [[ -n "$line" ]]; do
    case "$line" in
      \[*\])
        section="${line#[}"
        section="${section%]}"
        current_variant="${section%%:*}"
        current_parent=""
        if [[ "$section" == *:* ]]; then
          current_parent="${section#*:}"
          [[ "$current_parent" == "$current_variant" ]] && current_parent=""
        fi
        FSX_VARIANT_PARENT["${variant_path}::${current_variant}"]="${current_parent}"
        ;;
      maxFile\ =\ *)
        if [[ -n "$current_variant" ]]; then
          FSX_VARIANT_MAX_FILE["${variant_path}::${current_variant}"]="$(fsx_file_index "${line#maxFile = }")"
        fi
        ;;
      maxRank\ =\ *)
        if [[ -n "$current_variant" ]]; then
          FSX_VARIANT_MAX_RANK["${variant_path}::${current_variant}"]="$((10#${line#maxRank = } - 1))"
        fi
        ;;
    esac
  done < "$variant_path"

  FSX_VARIANT_META_LOADED["$variant_path"]=1
}

# Largest board limits over a variant and all its ancestors. A variant can
# only load when its whole inheritance chain (including templates) fits
# the build, so scope decisions must use the ancestral maximum rather
# than the nearest-defined limits.
fsx_variant_ancestral_limits() {
  local variant_path="$1"
  local variant="$2"
  local current="$variant"
  local key parent
  local max_file=-1
  local max_rank=-1
  local depth=0

  fsx_variant_load_metadata "$variant_path"

  while [[ -n "$current" && $depth -lt 32 ]]; do
    key="${variant_path}::${current}"
    if [[ -n "${FSX_VARIANT_MAX_FILE["$key"]:-}" ]]; then
      [[ "${FSX_VARIANT_MAX_FILE["$key"]}" -gt $max_file ]] && max_file="${FSX_VARIANT_MAX_FILE["$key"]}"
    fi
    if [[ -n "${FSX_VARIANT_MAX_RANK["$key"]:-}" ]]; then
      [[ "${FSX_VARIANT_MAX_RANK["$key"]}" -gt $max_rank ]] && max_rank="${FSX_VARIANT_MAX_RANK["$key"]}"
    fi
    parent="${FSX_VARIANT_PARENT["$key"]:-}"
    if [[ -z "$parent" || "$parent" == "$current" ]]; then
      break
    fi
    current="$parent"
    ((++depth))
  done

  [[ $max_file -lt 0 ]] && max_file=7
  [[ $max_rank -lt 0 ]] && max_rank=7
  printf '%s %s\n' "$max_file" "$max_rank"
}

fsx_variant_effective_limits() {
  local variant_path="$1"
  local variant="$2"
  local current="$variant"
  local key parent
  local max_file=-1
  local max_rank=-1
  local depth=0

  fsx_variant_load_metadata "$variant_path"

  while [[ -n "$current" && $depth -lt 32 ]]; do
    key="${variant_path}::${current}"
    if [[ $max_file -lt 0 && -n "${FSX_VARIANT_MAX_FILE["$key"]:-}" ]]; then
      max_file="${FSX_VARIANT_MAX_FILE["$key"]}"
    fi
    if [[ $max_rank -lt 0 && -n "${FSX_VARIANT_MAX_RANK["$key"]:-}" ]]; then
      max_rank="${FSX_VARIANT_MAX_RANK["$key"]}"
    fi
    parent="${FSX_VARIANT_PARENT["$key"]:-}"
    if [[ -z "$parent" || "$parent" == "$current" ]]; then
      break
    fi
    current="$parent"
    ((++depth))
  done

  [[ $max_file -lt 0 ]] && max_file=7
  [[ $max_rank -lt 0 ]] && max_rank=7
  printf '%s %s\n' "$max_file" "$max_rank"
}

fsx_build_variant_limits() {
  local engine="$1"
  local engine_basen="${engine##*/}"

  # Honor an explicit family override (run.sh and CI use this for
  # custom-named binaries); otherwise infer from the binary name.
  case "${FSX_ENGINE_FAMILY:-}" in
    very-large) echo "15 15"; return ;;
    large) echo "11 9"; return ;;
    normal) echo "7 7"; return ;;
  esac

  case "$engine_basen" in
    stockfish-large*|stockfish-allvars*)
      echo "11 9"
      ;;
    stockfish-vlb*)
      echo "15 15"
      ;;
    *)
      echo "7 7"
      ;;
  esac
}

# Expected suite board as "<maxFileIdx> <maxRankIdx>". run.sh exports
# FSX_EXPECTED_BOARD per suite from its board family; standalone runs
# default to the actual engine's limits.
fsx_expected_board_limits() {
  if [[ -n "${FSX_EXPECTED_BOARD:-}" ]]; then
    printf '%s\n' "${FSX_EXPECTED_BOARD}"
  else
    fsx_build_variant_limits "${1:-}"
  fi
}

# Classify variant availability for an engine without side effects.
# Prints one of: available, in-scope-missing, out-of-scope, missing.
# - available: the engine loads the variant.
# - in-scope-missing: board-size absence, but the variant fits the expected
#   suite board, so running here would silently drop promised coverage.
# - out-of-scope: board-size absence for a variant outside the expected
#   suite board (or blocked by a template that the expected board cannot
#   provide either); skipping is legitimate.
# - missing: absent for non-size reasons; always a regression.
fsx_variant_board_status() {
  local engine="$1"
  local variant="$2"
  local variant_path="${3:-${VARIANTS}}"
  local variant_file variant_rank build_file build_rank expected_file expected_rank

  if probe_variant_available "$engine" "$variant" "$variant_path"; then
    echo "available"
    return 0
  fi

  # Scope the absence against the ancestral maximum: children of skipped
  # templates (e.g. hex-7x7 of hex) cannot load even when their own
  # limits look small.
  read -r variant_file variant_rank < <(fsx_variant_ancestral_limits "$variant_path" "$variant")
  read -r build_file build_rank < <(fsx_build_variant_limits "$engine")
  if [[ $variant_file -gt $build_file || $variant_rank -gt $build_rank ]] \
      || fsx_variant_skipped_by_build_output "$variant" "$variant_path"; then
    read -r expected_file expected_rank < <(fsx_expected_board_limits "$engine")
    if [[ $variant_file -le $expected_file && $variant_rank -le $expected_rank ]] \
        && ! fsx_variant_template_blocked_same_board "$variant" "$variant_path" "$build_file" "$build_rank" "$expected_file" "$expected_rank"; then
      echo "in-scope-missing"
    else
      echo "out-of-scope"
    fi
    return 0
  fi

  echo "missing"
}

# True when the variant is blocked by a size-skipped template that the
# expected board cannot provide either (i.e. the actual and expected
# boards agree, so a larger suite engine would skip it just the same).
# Such variants are out of scope rather than dropped coverage.
fsx_variant_template_blocked_same_board() {
  local variant="$1"
  local variant_path="$2"
  local build_file="$3"
  local build_rank="$4"
  local expected_file="$5"
  local expected_rank="$6"
  local out="${FSX_VARIANT_PROBE_OUTPUT:-}"

  [[ $build_file -eq $expected_file && $build_rank -eq $expected_rank ]] || return 1
  [[ -n "$out" ]] || return 1
  fsx_variant_skipped_by_build_output "$variant" "$variant_path" "$out" || return 1

  # Only template-driven blocks qualify: the plain variants-skipped line
  # carries no names, so a match implies a template (ancestor) was listed.
  local summary_line
  summary_line=$(grep -E 'variant templates not found or skipped because of board size limits' <<<"$out" || true)
  [[ -n "$summary_line" ]] || return 1
  [[ "${summary_line}" == *"("* ]] || return 1

  local current key parent depth=0
  local names name
  local -a parsed_names
  current="$variant"
  fsx_variant_load_metadata "$variant_path"
  while [[ -n "$current" && $depth -lt 32 ]]; do
    key="${variant_path}::${current}"
    parent="${FSX_VARIANT_PARENT["$key"]:-}"
    if [[ -z "$parent" || "$parent" == "$current" ]]; then
      break
    fi
    names="${summary_line#*\(}"
    names="${names%%\)*}"
    IFS=',' read -r -a parsed_names <<<"${names}"
    for name in "${parsed_names[@]}"; do
      name="${name#"${name%%[![:space:]]*}"}"
      name="${name%"${name##*[![:space:]]}"}"
      [[ "${name}" == "${parent}" ]] && return 0
    done
    current="$parent"
    ((++depth))
  done

  return 1
}

# Enforce a board status: 0 when usable, 1 for a legitimate skip, loud
# exit 1 when coverage would be silently dropped. FSX_ALLOW_SMALL_BOARD=1
# downgrades loud failures to skips for intentional small-board runs.
fsx_enforce_board_status() {
  local status="$1"
  local engine="$2"
  local variant="$3"
  local variant_path="${4:-${VARIANTS:-}}"
  case "$status" in
    available)
      return 0
      ;;
    out-of-scope)
      echo "SKIP-BOARDSIZE: ${variant} is outside the expected suite board; skipping on ${engine##*/}" >&2
      return 1
      ;;
    in-scope-missing)
      if [[ "${FSX_ALLOW_SMALL_BOARD:-0}" == 1 ]]; then
        echo "SKIP-BOARDSIZE: ${variant} exceeds ${engine##*/} limits; skip allowed by FSX_ALLOW_SMALL_BOARD=1" >&2
        return 1
      fi
      echo "expected variant '${variant}' fits the suite board but ${engine##*/} cannot load it; refusing to silently drop coverage" >&2
      echo "run with a larger-board engine or set FSX_ALLOW_SMALL_BOARD=1 to allow the skip" >&2
      exit 1
      ;;
    *)
      echo "expected variant '${variant}' is missing from ${variant_path}" >&2
      echo "build target ${engine##*/} should provide it; treat this as a regression" >&2
      exit 1
      ;;
  esac
}

fsx_variant_exceeds_build_limits() {
  local engine="$1"
  local variant="$2"
  local variant_path="${3:-${VARIANTS}}"
  local variant_file variant_rank build_file build_rank

  read -r variant_file variant_rank < <(fsx_variant_effective_limits "$variant_path" "$variant")
  read -r build_file build_rank < <(fsx_build_variant_limits "$engine")

  [[ $variant_file -gt $build_file || $variant_rank -gt $build_rank ]]
}

fsx_variant_skipped_by_build_output() {
  local variant="$1"
  local variant_path="${2:-${VARIANTS}}"
  local out="${3:-${FSX_VARIANT_PROBE_OUTPUT:-}}"
  local summary_lines candidate
  local -a candidates

  if [[ -z "$out" ]]; then
    return 1
  fi

  summary_lines=$(grep -E 'variants skipped because of board size limits|variant templates not found or skipped because of board size limits' <<<"$out" || true)
  if [[ -z "$summary_lines" ]]; then
    return 1
  fi

  candidates=("$variant")
  fsx_variant_load_metadata "$variant_path"
  local current="$variant" key parent depth=0
  while [[ -n "$current" && $depth -lt 32 ]]; do
    key="${variant_path}::${current}"
    parent="${FSX_VARIANT_PARENT["$key"]:-}"
    if [[ -z "$parent" || "$parent" == "$current" ]]; then
      break
    fi
    candidates+=("$parent")
    current="$parent"
    ((++depth))
  done

  local candidate_name summary_line names name
  local -a parsed_names
  for candidate_name in "${candidates[@]}"; do
    while IFS= read -r summary_line; do
      [[ "${summary_line}" == *"("* ]] || continue
      names="${summary_line#*\(}"
      names="${names%%\)*}"
      IFS=',' read -r -a parsed_names <<<"${names}"
      for name in "${parsed_names[@]}"; do
        name="${name#"${name%%[![:space:]]*}"}"
        name="${name%"${name##*[![:space:]]}"}"
        [[ "${name}" == "${candidate_name}" ]] && return 0
      done
    done <<<"${summary_lines}"
  done

  return 1
}

variant_available() {
  local engine="$1"
  local variant="$2"
  local variant_path="${3:-${VARIANTS}}"

  fsx_enforce_board_status "$(fsx_variant_board_status "$engine" "$variant" "$variant_path")" "$engine" "$variant" "$variant_path"
}

cleanup_tmp_ini() {
  if [[ -n "${FSX_TMP_INI:-}" && -e "${FSX_TMP_INI}" ]]; then
    rm -f "${FSX_TMP_INI}"
  fi
  FSX_TMP_INI=
  TMP_VARIANTS=
}

create_tmp_ini() {
  cleanup_tmp_ini
  FSX_TMP_INI=$(mktemp "${TMPDIR:-/tmp}/fsx-uci-XXXXXX.ini")
  export FSX_TMP_INI
  TMP_VARIANTS="${FSX_TMP_INI}"
  export TMP_VARIANTS
}

init_tmp_ini() {
  create_tmp_ini
  fsx_add_exit_cleanup cleanup_tmp_ini
}

load_inline_variants() {
  create_tmp_ini
  cat >"${FSX_TMP_INI}"
  fsx_add_exit_cleanup cleanup_tmp_ini
}

uci_position_command() {
  local fen_or_startpos="$1"
  shift

  if [[ "$fen_or_startpos" == "startpos" ]]; then
    printf 'position startpos'
  else
    printf 'position fen %s' "$fen_or_startpos"
  fi

  if (($#)); then
    printf ' moves %s' "$*"
  fi
  printf '\n'
}

run_perft() {
  local variant="$1"
  local fen_or_startpos="$2"
  local depth="$3"

  run_uci "${ENGINE}" "${VARIANTS}" "${variant}" <<UCI
$(uci_position_command "${fen_or_startpos}")
go perft ${depth}
UCI
}

run_display() {
  local variant="$1"
  local fen_or_startpos="$2"
  shift 2

  run_uci "${ENGINE}" "${VARIANTS}" "${variant}" <<UCI
$(uci_position_command "${fen_or_startpos}" "$@")
d
UCI
}

engine_config_output() {
  if [[ -z "${FSX_ENGINE_CONFIG_OUTPUT:-}" ]]; then
    FSX_ENGINE_CONFIG_OUTPUT=$(make -C "${ROOT_DIR}/src" -s config-sanity)
    export FSX_ENGINE_CONFIG_OUTPUT
  fi
  printf '%s\n' "${FSX_ENGINE_CONFIG_OUTPUT}"
}

engine_config_value() {
  local key="$1"
  engine_config_output | sed -n "s/^${key}: //p" | tail -n1
}

run_engine_stdin() {
  local engine="$1"
  local input="$2"

  printf '%s' "$input" | uci_timeout "$engine" 2>&1
}

bench_nodes() {
  awk '/Nodes searched  : / {print $4}' | tail -n1
}

expect_engine_setup() {
  local spawn_args="${1:-}"

  printf '   set engine [lindex $argv 0]\n'
  printf '   spawn $engine%s\n' "${spawn_args:+ ${spawn_args}}"
  # Fail any later expect on timeout instead of passing vacuously. This must
  # come after spawn: expect_after binds to the current spawn id, and
  # defining it earlier breaks matching entirely.
  printf '   expect_after { timeout { exit 1 } }\n'
}

run_expect() {
  local timeout_seconds="${EXPECT_TIMEOUT:-20}"
  local exp_file output_file status
  local harness_header harness_trailer

  exp_file=$(mktemp "${TMPDIR:-/tmp}/fsx-expect-XXXXXX.exp")
  output_file=$(mktemp "${TMPDIR:-/tmp}/fsx-expect-XXXXXX.out")
  cat >"${exp_file}"

  # Harden every expect script: a bare `expect pattern` returns normally on
  # timeout or premature EOF, which would pass assertions vacuously. Align
  # Tcl's per-expect timeout with the outer timeout, and refuse a non-zero
  # engine exit at the end. (Timeout failures themselves are enforced by the
  # expect_after guard that expect_engine_setup emits after spawn.)
  harness_header=$(printf 'set timeout %s\n' "${timeout_seconds}")
  harness_trailer=$(printf '\nset fsx_wait_status [wait]\nif {[lindex $fsx_wait_status 3] != 0} { exit 1 }\n')
  {
    printf '%s\n' "${harness_header}"
    cat "${exp_file}"
    printf '%s\n' "${harness_trailer}"
  } >"${exp_file}.wrapped"
  mv -f "${exp_file}.wrapped" "${exp_file}"

  if timeout "${timeout_seconds}" expect "${exp_file}" "$@" >"${output_file}" 2>&1; then
    status=0
  else
    status=$?
    cat "${output_file}" >&2
  fi

  if (( status == 0 )); then
    cat "${output_file}"
  fi
  rm -f "${exp_file}" "${output_file}"
  return "${status}"
}


assert_contains() {
  local haystack="$1"
  local pattern="$2"
  local context="${3:-contains}"

  if ! grep -Eq "$pattern" <<<"$haystack"; then
    echo "expected output to ${context}: $pattern" >&2
    echo "actual output:" >&2
    printf '%s\n' "$haystack" >&2
    return 1
  fi
}

assert_not_contains() {
  local haystack="$1"
  local pattern="$2"
  local context="${3:-not contain}"

  if grep -Eq "$pattern" <<<"$haystack"; then
    echo "expected output to ${context}: $pattern" >&2
    echo "actual output:" >&2
    printf '%s\n' "$haystack" >&2
    return 1
  fi
}

assert_nodes() {
  local haystack="$1"
  local expected="$2"

  assert_contains "$haystack" "^Nodes searched: ${expected}$" "have exact node count"
}
