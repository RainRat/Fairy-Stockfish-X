#!/bin/bash
# check for errors under valgrind or sanitizers.

set -euo pipefail

error()
{
  echo "instrumented testing failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR
ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
SCRATCH_DIR="${ROOT_DIR}/.local/build/instrumented"
mkdir -p "${SCRATCH_DIR}"
cleanup() { [[ -z "${tsan_supp:-}" ]] || rm -f "${tsan_supp}"; rm -f "${game_exp:-}" "${syzygy_exp:-}" "${SCRATCH_DIR}"/expect-output.*; }
trap cleanup EXIT
if [[ "${1:-}" == --* ]]; then
  mode_arg=$1
  shift
else
  mode_arg=normal
fi
ENGINE=${1:-${ROOT_DIR}/src/stockfish}
[[ -x "${ENGINE}" ]] || { echo "engine is not executable: ${ENGINE}" >&2; exit 2; }

# define suitable post and prefixes for testing options
case ${mode_arg} in
  --valgrind)
    echo "valgrind testing started"
    mode="valgrind"
    valgrind_args=(--error-exitcode=42 --errors-for-leak-kinds=all --leak-check=full)
    threads="1"
  ;;
  --valgrind-thread)
    echo "valgrind-thread testing started"
    mode="valgrind-thread"
    valgrind_args=(--fair-sched=try --error-exitcode=42)
    threads="2"
  ;;
  --sanitizer-undefined)
    echo "sanitizer-undefined testing started"
    mode="sanitizer-undefined"
    pattern="runtime error:"
    threads="1"
  ;;
  --sanitizer-thread)
    echo "sanitizer-thread testing started"
    mode="sanitizer-thread"
    pattern="WARNING: ThreadSanitizer:"
    threads="2"

    tsan_supp=$(mktemp "${SCRATCH_DIR}/tsan-supp.XXXXXX")
    cat << EOF > "$tsan_supp"
race:Stockfish::TTEntry::move
race:Stockfish::TTEntry::depth
race:Stockfish::TTEntry::bound
race:Stockfish::TTEntry::save
race:Stockfish::TTEntry::value
race:Stockfish::TTEntry::eval
race:Stockfish::TTEntry::is_pv

race:Stockfish::TranspositionTable::probe
race:Stockfish::TranspositionTable::hashfull
EOF

    export TSAN_OPTIONS="suppressions=$tsan_supp"
  ;;
  *)
    echo "unknown testing started"
    mode="normal"
    threads="1"
  ;;
esac

run_test_cmd() {
  local args=("$@")
  if [[ "$mode" == "valgrind" || "$mode" == "valgrind-thread" ]]; then
    valgrind "${valgrind_args[@]}" "${ENGINE}" "${args[@]}" >/dev/null
  elif [[ "$mode" == "sanitizer-undefined" || "$mode" == "sanitizer-thread" ]]; then
    local out
    out=$("${ENGINE}" "${args[@]}" 2>&1)
    if grep -q "$pattern" <<<"$out"; then
      echo "Sanitizer warning/error detected:" >&2
      grep -A50 "$pattern" <<<"$out" >&2
      return 1
    fi
  else
    "${ENGINE}" "${args[@]}"
  fi
}

# simple command line testing
run_test_cmd eval
run_test_cmd go nodes 1000
run_test_cmd go depth 10
run_test_cmd go movetime 1000
run_test_cmd go wtime 8000 btime 8000 winc 500 binc 500
# Valgrind slows the full bench enough to trip a search assertion in CI, so keep
# the other coverage here and let non-valgrind modes exercise bench.
if [[ "$mode" != "valgrind" && "$mode" != "valgrind-thread" ]]; then
  run_test_cmd bench 128 "$threads" 8 default depth
fi

# more general testing, following an uci protocol exchange
game_exp=$(mktemp "${SCRATCH_DIR}/game.XXXXXX.exp")
printf -v engine_q '%q' "${ENGINE}"
spawn_cmd="${engine_q}"
if [[ "$mode" == "valgrind" ]]; then
  spawn_cmd="valgrind --error-exitcode=42 --errors-for-leak-kinds=all --leak-check=full ${engine_q}"
elif [[ "$mode" == "valgrind-thread" ]]; then
  spawn_cmd="valgrind --fair-sched=try --error-exitcode=42 ${engine_q}"
fi

cat << EOF > "$game_exp"
 set timeout 240
 spawn $spawn_cmd

 proc expect_bestmove {} {
     expect {
         -re "bestmove\[^\\r\\n\]*(\\r\\n|\\n)" {}
         eof {exit 1}
         timeout {exit 1}
     }
 }

 send "uci\n"
 expect "uciok"

 send "setoption name Threads value $threads\n"

 send "ucinewgame\n"
 send "position startpos\n"
 send "go nodes 1000\n"
 expect_bestmove

 send "position startpos moves e2e4 e7e6\n"
 send "go nodes 1000\n"
 expect_bestmove

 send "position fen 5rk1/1K4p1/8/8/3B4/8/8/8 b - - 0 1\n"
 send "go depth 10\n"
 expect_bestmove

 send "quit\n"
 expect eof

 # return error code of the spawned program, useful for valgrind
 lassign [wait] pid spawnid os_error_flag value
 exit \$value
EOF

# Syzygy data is user-provided and never downloaded or changed by this test.
SYZYGY_DIR=${FSX_SYZYGY_DIR:-${ROOT_DIR}/tests/syzygy}
if [[ ! -d "${SYZYGY_DIR}" ]]; then
  echo "SKIP: Syzygy instrumented probe requires local tablebases; set FSX_SYZYGY_DIR." >&2
fi

syzygy_exp=$(mktemp "${SCRATCH_DIR}/syzygy.XXXXXX.exp")
cat << EOF > "$syzygy_exp"
 set timeout 600
 spawn $spawn_cmd
 send "uci\n"
 send "setoption name SyzygyPath value ${SYZYGY_DIR}/\n"
 expect "info string Found 35 tablebases" {} timeout {exit 1}
 send "bench 128 1 8 default depth\n"
 send "quit\n"
 expect eof

 # return error code of the spawned program, useful for valgrind
 lassign [wait] pid spawnid os_error_flag value
 exit \$value
EOF

run_expect_test() {
  local exp="$1"
  if [[ "$mode" == "valgrind" || "$mode" == "valgrind-thread" ]]; then
    local out
    out=$(mktemp "${SCRATCH_DIR}/expect-output.XXXXXX")
    if ! expect "$exp" >"$out" 2>&1; then
      cat "$out" >&2
      rm -f "$out"
      return 1
    fi
    rm -f "$out"
  elif [[ "$mode" == "sanitizer-undefined" || "$mode" == "sanitizer-thread" ]]; then
    local out
    out=$(expect "$exp" 2>&1)
    if grep -q "$pattern" <<<"$out"; then
      echo "Sanitizer warning/error detected in expect script:" >&2
      grep -A50 "$pattern" <<<"$out" >&2
      return 1
    fi
  else
    expect "$exp"
  fi
}

if [[ -d "${SYZYGY_DIR}" ]]; then
  instrumented_exps=("$game_exp" "$syzygy_exp")
else
  instrumented_exps=("$game_exp")
fi
for exp in "${instrumented_exps[@]}"
do
  echo "Running expect test: $exp"
  run_expect_test "$exp"
  rm "$exp"
done

echo "instrumented testing OK"
