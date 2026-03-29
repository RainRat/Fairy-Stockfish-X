#!/usr/bin/env python3
import pyffish as sf


INI = """
[vlb-token-san:fairy]
maxRank = 5
maxFile = 5
customPiece1 = a':W
startFen = 4k/5/5/a'4/A'3K w - - 0 1
"""


def main() -> None:
    sf.load_variant_config(INI)
    fen = sf.start_fen("vlb-token-san")

    san = sf.get_san("vlb-token-san", fen, "a1b1")
    if san != "A'b1":
        raise SystemExit(f"unexpected SAN for tokenized piece: {san!r}")

    san_moves = sf.get_san_moves("vlb-token-san", fen, ["a1b1"])
    if san_moves != ["A'b1"]:
        raise SystemExit(f"unexpected SAN move list: {san_moves!r}")

    partner = sf.piece_to_partner("vlb-token-san", fen, ["a1a2"])
    if partner != "a'":
        raise SystemExit(f"unexpected captured partner symbol: {partner!r}")


if __name__ == "__main__":
    main()
