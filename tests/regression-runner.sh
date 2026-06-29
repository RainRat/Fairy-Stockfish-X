#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
SCRIPT_PATH="${ROOT_DIR}/tests/regression-runner.sh"
STATE_DIR="${ROOT_DIR}/.local/regression"
RUNS_DIR="${STATE_DIR}/runs"
CURRENT_FILE="${STATE_DIR}/current"

usage() {
  cat <<'EOF'
Usage: tests/regression-runner.sh start [engine]
       tests/regression-runner.sh status
       tests/regression-runner.sh wait
       tests/regression-runner.sh run [engine]
       tests/regression-runner.sh log

Runs tests/local-regression.sh detached, preserving one full log while status and
wait produce concise output. The default engine is src/stockfish-large.
EOF
}

current_run_dir() {
  [[ -f "${CURRENT_FILE}" ]] || return 1
  cat "${CURRENT_FILE}"
}

run_is_alive() {
  local run_dir="$1" pid
  [[ ! -f "${run_dir}/exit" && -f "${run_dir}/pid" ]] || return 1
  pid=$(<"${run_dir}/pid")
  kill -0 "${pid}" 2>/dev/null
}

engine_is_stale() {
  local engine="$1"
  find "${ROOT_DIR}/src" -type f \( -name '*.cpp' -o -name '*.h' -o -name 'Makefile' \) \
    -newer "${engine}" -print -quit | grep -q .
}

validate_engines() {
  local primary="$1" candidate stale=0
  local candidates=(
    "${primary}"
    "${VLB_ENGINE:-${ROOT_DIR}/src/stockfish-vlb}"
    "${LARGE_ENGINE:-${ROOT_DIR}/src/stockfish-large}"
    "${MINI_ENGINE:-${ROOT_DIR}/src/stockfish-allvars}"
  )

  for candidate in "${candidates[@]}"; do
    [[ "${candidate}" == /* ]] || candidate="${ROOT_DIR}/${candidate}"
    [[ -x "${candidate}" ]] || continue
    if engine_is_stale "${candidate}"; then
      echo "stale engine: ${candidate}" >&2
      stale=1
    fi
  done

  if (( stale )); then
    echo "rebuild the named regression binaries before starting the suite" >&2
    return 2
  fi
}

elapsed_seconds() {
  local run_dir="$1" start end
  start=$(<"${run_dir}/start")
  if [[ -f "${run_dir}/duration" ]]; then
    cat "${run_dir}/duration"
    return
  fi
  end=$(date +%s)
  echo $((end - start))
}

format_duration() {
  local seconds=${1%.*}
  printf '%02d:%02d:%02d' $((seconds / 3600)) $(((seconds % 3600) / 60)) $((seconds % 60))
}

historical_total() {
  local run run_exit duration log value count=0 sum=0

  if [[ -d "${RUNS_DIR}" ]]; then
    while IFS= read -r run; do
      [[ -f "${run}/exit" && -f "${run}/duration" ]] || continue
      run_exit=$(<"${run}/exit")
      [[ "${run_exit}" == "0" ]] || continue
      duration=$(<"${run}/duration")
      sum=$((sum + ${duration%.*}))
      count=$((count + 1))
      (( count == 5 )) && break
    done < <(find "${RUNS_DIR}" -mindepth 1 -maxdepth 1 -type d -print | sort -r)
  fi

  if (( count < 5 )) && [[ -d "${ROOT_DIR}/.local/logs" ]]; then
    while IFS= read -r log; do
      grep -q '^local regression suite passed$' "${log}" || continue
      value=$(awk '/^total elapsed [0-9.]+s$/ {v=$3} END {sub(/s$/, "", v); print v}' "${log}")
      [[ -n "${value}" ]] || continue
      sum=$((sum + ${value%.*}))
      count=$((count + 1))
      (( count == 5 )) && break
    done < <(find "${ROOT_DIR}/.local/logs" -maxdepth 1 -type f -name 'local-regression*.log' -print | sort -r)
  fi

  (( count > 0 )) || return 1
  echo $((sum / count))
}

recommended_interval() {
  local run_dir="$1" elapsed estimate remaining interval
  elapsed=$(elapsed_seconds "${run_dir}")
  if estimate=$(historical_total); then
    remaining=$((estimate - elapsed))
    (( remaining < 0 )) && remaining=0
    interval=$((remaining / 4))
    (( interval < 30 )) && interval=30
    (( interval > 300 )) && interval=300
  else
    interval=60
  fi
  echo "${interval}"
}

current_step() {
  local log="$1" step
  step=$(grep '^== .* ==$' "${log}" 2>/dev/null | tail -n1 || true)
  step=${step#== }
  step=${step% ==}
  printf '%s' "${step:-starting}"
}

relative_path() {
  local path="$1"
  printf '%s' "${path#"${ROOT_DIR}/"}"
}

status_run() {
  local run_dir pid elapsed log step estimate remaining interval rc
  if ! run_dir=$(current_run_dir); then
    echo "no regression run"
    return
  fi

  log="${run_dir}/regression.log"
  elapsed=$(elapsed_seconds "${run_dir}")
  step=$(current_step "${log}")

  if [[ -f "${run_dir}/exit" ]]; then
    rc=$(<"${run_dir}/exit")
    if [[ "${rc}" == "0" ]]; then
      printf 'passed elapsed=%s log=%s\n' "$(format_duration "${elapsed}")" "$(relative_path "${log}")"
    else
      printf 'failed exit=%s elapsed=%s step="%s" log=%s\n' \
        "${rc}" "$(format_duration "${elapsed}")" "${step}" "$(relative_path "${log}")"
    fi
    return
  fi

  pid=$(<"${run_dir}/pid")
  if ! run_is_alive "${run_dir}"; then
    printf 'interrupted pid=%s elapsed=%s step="%s" log=%s\n' \
      "${pid}" "$(format_duration "${elapsed}")" "${step}" "$(relative_path "${log}")"
    return
  fi

  interval=$(recommended_interval "${run_dir}")
  if estimate=$(historical_total); then
    remaining=$((estimate - elapsed))
    (( remaining < 0 )) && remaining=0
    printf 'running pid=%s elapsed=%s step="%s" estimated_remaining=%s check_again=%ss log=%s\n' \
      "${pid}" "$(format_duration "${elapsed}")" "${step}" "$(format_duration "${remaining}")" \
      "${interval}" "$(relative_path "${log}")"
  else
    printf 'running pid=%s elapsed=%s step="%s" check_again=%ss log=%s\n' \
      "${pid}" "$(format_duration "${elapsed}")" "${step}" "${interval}" "$(relative_path "${log}")"
  fi
}

start_run() {
  local engine=${1:-src/stockfish-large} run_id run_dir pid
  if [[ "${engine}" != /* ]]; then
    engine="${ROOT_DIR}/${engine}"
  fi
  [[ -x "${engine}" ]] || { echo "engine is not executable: ${engine}" >&2; return 2; }
  validate_engines "${engine}"

  if run_dir=$(current_run_dir 2>/dev/null) && run_is_alive "${run_dir}"; then
    echo "regression already running (pid $(<"${run_dir}/pid"))" >&2
    return 2
  fi

  mkdir -p "${RUNS_DIR}"
  run_id="$(date +%Y%m%d-%H%M%S)-$$"
  run_dir="${RUNS_DIR}/${run_id}"
  mkdir -p "${run_dir}"
  printf '%s\n' "${run_dir}" > "${CURRENT_FILE}"
  printf '%s\n' "${engine}" > "${run_dir}/engine"
  date +%s > "${run_dir}/start"

  if command -v setsid >/dev/null 2>&1; then
    setsid "${SCRIPT_PATH}" _run "${run_dir}" "${engine}" >/dev/null 2>&1 < /dev/null &
  else
    nohup "${SCRIPT_PATH}" _run "${run_dir}" "${engine}" >/dev/null 2>&1 < /dev/null &
  fi
  pid=$!
  printf '%s\n' "${pid}" > "${run_dir}/pid"
  printf 'started pid=%s log=%s\n' "${pid}" "$(relative_path "${run_dir}/regression.log")"
}

wait_run() {
  local run_dir rc
  run_dir=$(current_run_dir) || { echo "no regression run"; return 2; }

  while run_is_alive "${run_dir}"; do
    sleep 5
  done

  status_run
  if [[ ! -f "${run_dir}/exit" ]]; then
    return 1
  fi
  rc=$(<"${run_dir}/exit")
  if [[ "${rc}" != "0" ]]; then
    echo "--- failure tail ---"
    tail -80 "${run_dir}/regression.log"
  fi
  return "${rc}"
}

worker_run() {
  local run_dir="$1" engine="$2" started ended rc
  printf '%s\n' "$$" > "${run_dir}/pid"
  started=$(<"${run_dir}/start")
  set +e
  /usr/bin/time -f "total elapsed %es" bash "${ROOT_DIR}/tests/local-regression.sh" "${engine}" \
    > "${run_dir}/regression.log" 2>&1
  rc=$?
  set -e
  ended=$(date +%s)
  printf '%s\n' "$((ended - started))" > "${run_dir}/duration"
  printf '%s\n' "${rc}" > "${run_dir}/exit.tmp"
  mv "${run_dir}/exit.tmp" "${run_dir}/exit"
  return "${rc}"
}

command=${1:-}
case "${command}" in
  start)
    shift
    start_run "$@"
    ;;
  status)
    status_run
    ;;
  wait)
    wait_run
    ;;
  run)
    shift
    start_run "$@"
    wait_run
    ;;
  log)
    run_dir=$(current_run_dir) || { echo "no regression run"; exit 2; }
    tail -80 "${run_dir}/regression.log"
    ;;
  _run)
    worker_run "$2" "$3"
    ;;
  -h|--help|help)
    usage
    ;;
  *)
    usage >&2
    exit 2
    ;;
esac
