#!/usr/bin/env python3

import re
import subprocess
import sys


engine, variants = sys.argv[1:3]
proc = subprocess.Popen(
    [engine], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1
)


def send(command):
    proc.stdin.write(command + "\n")
    proc.stdin.flush()


def read_until(prefix):
    lines = []
    while True:
        line = proc.stdout.readline()
        if not line:
            raise RuntimeError("engine exited before " + prefix)
        lines.append(line.rstrip())
        if line.startswith(prefix):
            return lines


try:
    send("uci")
    read_until("uciok")
    send("setoption name VariantPath value " + variants)
    send("setoption name UCI_Variant value dos-laser-chess")
    send("isready")
    read_until("readyok")
    send("position startpos")

    for depth in range(1, 5):
        send("go depth " + str(depth))
        output = read_until("bestmove ")
        if output[-1] == "bestmove (none)":
            raise AssertionError("repeated go returned no move at depth " + str(depth))
        if depth > 1 and not any(" nodes " in line and " nodes 0 " not in line for line in output):
            raise AssertionError("repeated go searched no nodes at depth " + str(depth))
        if depth == 1:
            counts = [int(match.group(1)) for line in output if (match := re.search(r" nodes (\d+)", line))]
            if not counts or counts[-1] >= 600:
                raise AssertionError("laser rotation search hint was not applied")

    send("d")
    display = read_until("Checkers:")
    fen = next((line for line in display if line.startswith("Fen: ")), "")
    if " w - - 0 1" not in fen:
        raise AssertionError("repeated go changed the root position: " + fen)
finally:
    if proc.poll() is None:
        try:
            send("quit")
            proc.wait(timeout=10)
        except (BrokenPipeError, subprocess.TimeoutExpired):
            proc.kill()
            proc.wait()
