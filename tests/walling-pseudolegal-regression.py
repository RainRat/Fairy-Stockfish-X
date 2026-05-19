import pyffish

def test_walling_pseudolegal_roundtrip():
    ini = """[walling-regression-test:atomic]
wallingRule = duck"""
    pyffish.load_variant_config(ini)

    variant = "walling-regression-test"
    fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

    walling_move = "e2e3,e3a3"

    result_fen = pyffish.get_fen(variant, fen, [walling_move])
    assert "*" in result_fen, "Wall was not applied in FEN"

    moves = pyffish.legal_moves(variant, result_fen, [])
    assert len(moves) > 0, "No legal moves after walling"

if __name__ == "__main__":
    test_walling_pseudolegal_roundtrip()
    print("Success")
