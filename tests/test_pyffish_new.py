import pyffish
import unittest

class TestPyFFishNew(unittest.TestCase):
    def test_from_san(self):
        # Normal move
        self.assertEqual(pyffish.from_san("chess", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "e4"), "e2e4")
        # Capture
        self.assertEqual(pyffish.from_san("chess", "rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2", "exd5"), "e4d5")
        # Promotion
        self.assertEqual(pyffish.from_san("chess", "8/4P3/8/8/8/8/8/4K2k w - - 0 1", "e8=Q"), "e7e8q")
        # Invalid
        self.assertIsNone(pyffish.from_san("chess", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "e5"))

    def test_legal_moves_notation(self):
        moves = pyffish.legal_moves("chess", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", [], False, pyffish.NOTATION_SAN)
        self.assertIn("e4", moves)
        self.assertIn("Nf3", moves)

if __name__ == "__main__":
    unittest.main()
