#!/usr/bin/env python3
"""Check the compiled DLL capacity and a boundary move through its public API."""
import ctypes
import pathlib
import sys

dll = ctypes.CDLL(str(pathlib.Path(sys.argv[1]).resolve()))
bits = int(sys.argv[2])
width, height, banner = {64: (8, 8, ""), 128: (12, 10, " LB "),
                       256: (16, 16, " VLB "), 512: (22, 22, " HB 512 ")}[bits]


def function(name, result, *args):
    fn = getattr(dll, name)
    fn.restype, fn.argtypes = result, args
    return fn


ptr, string = ctypes.c_void_p, ctypes.c_char_p
free = function("fsf_free", None, ptr)


def text(name, *args):
    value = function(name, ptr, *([ptr, ctypes.c_bool, ctypes.c_int] if args else []))(*args)
    try:
        return ctypes.string_at(value).decode()
    finally:
        free(value)


info = text("fsf_info")
assert banner in info, info
if bits == 64:
    assert not any(marker in info for marker in (" LB ", " VLB ", " HB ")), info
start = f"{width - 1}k/{width - 2}R1/" + f"{width}/" * (height - 3) + f"K{width - 1} w - - 0 1"
load = function("fsf_try_load_variant_config", ctypes.c_int, string)
new = function("fsf_new_board", ptr, string, string, ctypes.c_bool)
push = function("fsf_push", ctypes.c_bool, ptr, string)
pop = function("fsf_pop", None, ptr)
release = function("fsf_free_board", None, ptr)
assert load((f"[dll-capacity:chess]\nmaxFile = {width}\nmaxRank = {height}\n"
             f"castling = false\nallowChecks = true\nstartFen = {start}\n").encode()) == 1
board = new(b"dll-capacity", b"", False)
assert board, info
try:
    move = f"{chr(97 + width - 2)}{height - 1}{chr(97 + width - 2)}{height}"
    assert push(board, move.encode()), move
    expected = f"{width - 2}Rk/" + f"{width}/" * (height - 2) + f"K{width - 1} b - - 1 1"
    assert text("fsf_fen", board, False, 0) == expected
    pop(board)
    assert text("fsf_fen", board, False, 0) == start
finally:
    release(board)
assert load((f"[dll-oversized:chess]\nmaxFile = {width + 1}\nmaxRank = {height}\n"
             "castling = false\n").encode()) == 0
assert not new(b"dll-oversized", b"", False), info
print(f"DLL {bits}-bit banner, maximum geometry, boundary move/undo and size rejection passed")
