#!/bin/bash
# check for errors under valgrind or sanitizers.

set -euo pipefail

error()
{
  echo "instrumented testing failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

# define suitable post and prefixes for testing options
mode=""
case $1 in
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

    tsan_supp=$(mktemp)
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
    valgrind "${valgrind_args[@]}" ./stockfish "${args[@]}" >/dev/null
  elif [[ "$mode" == "sanitizer-undefined" || "$mode" == "sanitizer-thread" ]]; then
    local out
    out=$(./stockfish "${args[@]}" 2>&1)
    if grep -q "$pattern" <<<"$out"; then
      echo "Sanitizer warning/error detected:" >&2
      grep -A50 "$pattern" <<<"$out" >&2
      return 1
    fi
  else
    ./stockfish "${args[@]}"
  fi
}

# simple command line testing
run_test_cmd eval
run_test_cmd go nodes 1000
run_test_cmd go depth 10
run_test_cmd go movetime 1000
run_test_cmd go wtime 8000 btime 8000 winc 500 binc 500
run_test_cmd bench 128 "$threads" 8 default depth

# more general testing, following an uci protocol exchange
game_exp=$(mktemp)
spawn_cmd="./stockfish"
if [[ "$mode" == "valgrind" ]]; then
  spawn_cmd="valgrind --error-exitcode=42 --errors-for-leak-kinds=all --leak-check=full ./stockfish"
elif [[ "$mode" == "valgrind-thread" ]]; then
  spawn_cmd="valgrind --fair-sched=try --error-exitcode=42 ./stockfish"
fi

cat << EOF > "$game_exp"
 set timeout 240
 spawn $spawn_cmd

 send "uci\n"
 expect "uciok"

 send "setoption name Threads value $threads\n"

 send "ucinewgame\n"
 send "position startpos\n"
 send "go nodes 1000\n"
 expect "bestmove"

 send "position startpos moves e2e4 e7e6\n"
 send "go nodes 1000\n"
 expect "bestmove"

 send "position fen 5rk1/1K4p1/8/8/3B4/8/8/8 b - - 0 1\n"
 send "go depth 10\n"
 expect "bestmove"

 send "quit\n"
 expect eof

 # return error code of the spawned program, useful for valgrind
 lassign [wait] pid spawnid os_error_flag value
 exit \$value
EOF

#download TB as needed
if [ ! -d ../tests/syzygy ]; then
   tmp_file=$(mktemp)
   curl -sL https://api.github.com/repos/niklasf/python-chess/tarball/9b9aa13f9f36d08aadfabff872882f4ab1494e95 -o "$tmp_file"
   expected_sha256="13d214c62dac85f854e28d8929707c81f4040ed437e11e5224338a287a3ea6cf"
   actual_sha256=$(sha256sum "$tmp_file" | awk '{print $1}')
   if [ "$expected_sha256" != "$actual_sha256" ]; then
      echo "Checksum mismatch for python-chess tarball!" >&2
      rm -f "$tmp_file"
      exit 1
   fi
   tar -xzf "$tmp_file"
   rm -f "$tmp_file"
   mv niklasf-python-chess-9b9aa13 ../tests/syzygy
fi

syzygy_exp=$(mktemp)
cat << EOF > "$syzygy_exp"
 set timeout 600
 spawn $spawn_cmd
 send "uci\n"
 send "setoption name SyzygyPath value ../tests/syzygy/\n"
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
    expect "$exp" >/dev/null
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

for exp in "$game_exp" "$syzygy_exp"
do
  echo "Running expect test: $exp"
  run_expect_test "$exp"
  rm "$exp"
done

if [ -n "${tsan_supp:-}" ]; then
  rm -f "$tsan_supp"
fi

echo "instrumented testing OK"
