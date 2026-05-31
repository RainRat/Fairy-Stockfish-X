import pyffish
import unittest

class TestSanUci(unittest.TestCase):
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

if __name__ == "__main__":
    unittest.main()
