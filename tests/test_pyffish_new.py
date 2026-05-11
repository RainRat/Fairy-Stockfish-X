import pyffish
import unittest

class TestNewFeatures(unittest.TestCase):
    def test_from_san(self):
        variant = "chess"
        fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

        # Test standard move
        self.assertEqual(pyffish.from_san(variant, fen, "e4"), "e2e4")
        self.assertEqual(pyffish.from_san(variant, fen, "Nf3"), "g1f3")

        # Test invalid move
        with self.assertRaises(ValueError):
            pyffish.from_san(variant, fen, "e5")

    def test_legal_moves_notation(self):
        variant = "chess"
        fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

        # Default UCI
        legal_uci = pyffish.legal_moves(variant, fen, [])
        self.assertIn("e2e4", legal_uci)
        self.assertNotIn("e4", legal_uci)

        # Explicit SAN
        legal_san = pyffish.legal_moves(variant, fen, [], False, pyffish.NOTATION_SAN)
        self.assertIn("e4", legal_san)
        self.assertNotIn("e2e4", legal_san)

    def test_position_san_fallback(self):
        # This is more of an engine test, but we can test it via pyffish.get_fen
        # which uses UCI::to_move internally.
        variant = "chess"
        fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

        # get_fen takes a movelist. We want to see if it accepts SAN.
        # Actually, pyffish.get_fen in pyffish.cpp uses buildPosition,
        # which uses UCI::to_move.
        fen_after = pyffish.get_fen(variant, fen, ["e4", "e5", "Nf3", "Nc6"])
        expected_fen = "r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3"
        self.assertTrue(fen_after.startswith(expected_fen.split(' ')[0]))

if __name__ == "__main__":
    unittest.main()
