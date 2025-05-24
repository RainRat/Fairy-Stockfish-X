#!/bin/bash
gdb -batch -q -x /app/gdb_commands.txt /app/src/stockfish > /app/gdb_output.txt 2>&1
