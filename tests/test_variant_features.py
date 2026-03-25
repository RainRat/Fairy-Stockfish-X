import unittest
import os
import pyffish as sf

class TestVariantFeatures(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # Load the default variants to have them available
        repo_root = os.environ.get("FSX_REPO_ROOT", "..")
        variants_ini = os.path.join(repo_root, "src", "variants.ini")
        if os.path.exists(variants_ini):
            with open(variants_ini, "r", encoding="utf-8") as f:
                sf.load_variant_config(f.read())

    def test_achi(self):
        # From tests/achi.sh
        center = sf.legal_moves('achi', '3/1P1/3 w - - 0 1', [])
        self.assertCountEqual(center, ['b2a1', 'b2b1', 'b2c1', 'b2a2', 'b2c2', 'b2a3', 'b2b3', 'b2c3'])

        edge = sf.legal_moves('achi', '3/3/1P1 w - - 0 1', [])
        self.assertCountEqual(edge, ['b1a1', 'b1c1', 'b1b2'])

        corner = sf.legal_moves('achi', '3/3/P2 w - - 0 1', [])
        self.assertCountEqual(corner, ['a1b1', 'a1a2', 'a1b2'])

        blocked_by_enemy = sf.legal_moves('achi', '3/1Pp/3 w - - 0 1', [])
        self.assertNotIn('b2c2', blocked_by_enemy)
        self.assertCountEqual(blocked_by_enemy, ['b2a1', 'b2b1', 'b2c1', 'b2a2', 'b2a3', 'b2b3', 'b2c3'])

    def test_custom_en_passant_passed_squares(self):
        # From tests/custom-en-passant-passed-squares.sh
        cfg = """
[custom-ep-all:chess]
customPiece1 = a:mWifemR3
customPiece2 = s:fK
pawn = -
enPassantTypes = as
tripleStepRegionWhite = *2
tripleStepRegionBlack = *7
enPassantRegionWhite = *1 *2 *3 *4 *5 *6 *7 *8
enPassantRegionBlack = *1 *2 *3 *4 *5 *6 *7 *8
startFen = 8/8/8/2s1s3/8/8/3A4/8 w - - 0 1
checking = false
flagPiece = -

[custom-ep-first:custom-ep-all]
enPassantPassedSquares = first
"""
        sf.load_variant_config(cfg)

        fen = sf.start_fen("custom-ep-all")
        fen_all = sf.get_fen("custom-ep-all", fen, ["d2d5"])
        self.assertIn(" b - d3d4d5 ", fen_all)
        self.assertTrue(sf.is_capture("custom-ep-all", fen_all, [], "c5d4"))
        self.assertTrue(sf.is_capture("custom-ep-all", fen_all, [], "e5d4"))

        fen_first = sf.get_fen("custom-ep-first", fen, ["d2d5"])
        self.assertIn(" b - d3 ", fen_first)
        self.assertFalse(sf.is_capture("custom-ep-first", fen_first, [], "c5d4"))
        self.assertFalse(sf.is_capture("custom-ep-first", fen_first, [], "e5d4"))

    def test_largeboard_seirawan(self):
        # From tests/largeboard-seirawan.sh
        cfg = """
[seirawan10:chess]
gating = true
seirawanGating = true
maxRank = 10
maxFile = 10
customPiece1 = h:N
pieceToCharTable = H:h
startFen = rnbqkbnr2/pppppppppp/10/10/10/10/10/10/PPPPPPPPPP/RNBQKBNR1R[Hh] w KQ|1000100001/0000000000 - 0 1
"""
        sf.load_variant_config(cfg)
        fen = sf.start_fen("seirawan10")
        self.assertEqual(sf.validate_fen(fen, "seirawan10", False), sf.FEN_OK)
        moves = sf.legal_moves("seirawan10", fen, [])
        self.assertIn("j1i1h", moves)
        self.assertIn("a2a3h", moves)
        self.assertEqual(sf.get_fen("seirawan10", fen, []), fen)
        after = sf.get_fen("seirawan10", fen, ["j1i1h"])
        self.assertIn("[h]", after)
        self.assertIn("|0000000000/0000000000", after)

    def test_wrapping_topology(self):
        # From tests/wrapping-topology.sh
        sf.load_variant_config(
            """
[cyl-checkmove:chess]
cylindrical = true
castling = false
startFen = 8/8/8/8/8/8/4K3/6Rk w - - 0 1
"""
        )
        fen = "8/8/8/8/8/8/4K3/6Rk w - - 0 1"
        self.assertFalse(sf.gives_check("cyl-checkmove", fen, []))
        self.assertTrue(sf.gives_check("cyl-checkmove", fen, ["g1a1"]))

    def test_castling_diag_validation(self):
        # From tests/parser-regressions.sh
        sf.load_variant_config(
            """
[castdiag-empty:chess]
maxFile = j
castling = true
castlingKingFile = f
castlingKingsideFile = i
castlingQueensideFile = c
castlingRookKingsideFile = j
castlingRookQueensideFile = b
startFen = 10/10/10/10/10/10/10/1R3K2R1 w JQ - 0 1

[castdiag-wrongpiece:chess]
maxFile = j
castling = true
castlingKingFile = f
castlingKingsideFile = i
castlingQueensideFile = c
castlingRookKingsideFile = j
castlingRookQueensideFile = b
startFen = 10/10/10/10/10/10/10/1R3K3N w JQ - 0 1
"""
        )
        # These FENs should be invalid with specific errors, but validate_fen only returns int.
        # The original test checked stderr output of the engine.
        # Since we use pyffish here, we just check that they are not FEN_OK.
        self.assertNotEqual(sf.validate_fen("10/10/10/10/10/10/10/1R3K2R1 w JQ - 0 1", "castdiag-empty", False), sf.FEN_OK)
        self.assertNotEqual(sf.validate_fen("10/10/10/10/10/10/10/1R3K3N w JQ - 0 1", "castdiag-wrongpiece", False), sf.FEN_OK)

if __name__ == '__main__':
    unittest.main()
