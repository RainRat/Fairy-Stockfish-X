import unittest

import pyffish as sf


class TestRoyalCaptureNoKings(unittest.TestCase):
    def test_royal_capture(self):
        sf.load_variant_config(
            """
[noroyal-capture:chess]
king = c:K
allowChecks = true
"""
        )
        fen = "4c3/8/8/8/8/8/4R3/4C3 w - - 0 1"
        moves = sf.legal_moves("noroyal-capture", fen, [])
        self.assertIn("e2e8", moves)
        self.assertTrue(sf.is_capture("noroyal-capture", fen, [], "e2e8"))
        is_end, result = sf.is_immediate_game_end("noroyal-capture", fen, ["e2e8"])
        self.assertTrue(is_end)
        self.assertNotEqual(result, sf.VALUE_NONE)


if __name__ == "__main__":
    unittest.main(verbosity=2)
