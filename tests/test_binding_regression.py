import sys
import unittest
from pathlib import Path

ROOT_DIR = Path(__file__).resolve().parents[1]
if str(ROOT_DIR) not in sys.path:
    sys.path.insert(0, str(ROOT_DIR))

import pyffish as sf

class TestBindings(unittest.TestCase):
    def test_is_capture_invalid_move(self):
        with self.assertRaises(ValueError):
            sf.is_capture("chess", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", [], "invalid")

    def test_game_result_not_terminal(self):
        res = sf.game_result("chess", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", [])
        self.assertEqual(res, sf.VALUE_NONE)

    def test_enclosing_drop_startpos_not_drawn_by_insufficient_material(self):
        with open(ROOT_DIR / "src" / "variants.ini", "r", encoding="utf-8") as f:
            sf.load_variant_config(f.read())
        fen = sf.start_fen("snort")
        self.assertTrue(sf.legal_moves("snort", fen, []))
        self.assertEqual(sf.game_result("snort", fen, []), sf.VALUE_NONE)
        self.assertEqual(sf.has_insufficient_material("snort", fen, []), (False, False))

    def test_brandub_missing_king_is_not_draw(self):
        with open(ROOT_DIR / "src" / "variants.ini", "r", encoding="utf-8") as f:
            sf.load_variant_config(f.read())
        fen = "7/7/3r3/2r1r2/3R3/7/7 w - - 0 1"
        self.assertLess(sf.game_result("brandub", fen, []), 0)

    def test_antiminishogi_startpos_not_terminal(self):
        with open(ROOT_DIR / "src" / "variants.ini", "r", encoding="utf-8") as f:
            sf.load_variant_config(f.read())
        fen = "rbsgk/4p/5/P4/KGSBR[] w - - 0 1"
        self.assertEqual(sf.game_result("antiminishogi", fen, []), sf.VALUE_NONE)
        self.assertTrue(sf.legal_moves("antiminishogi", fen, []))

    def test_anti_losalamos_missing_queen_not_terminal(self):
        with open(ROOT_DIR / "src" / "variants.ini", "r", encoding="utf-8") as f:
            sf.load_variant_config(f.read())
        fen = "rn1knr/pppppp/6/6/PPPPPP/RNQKNR w - - 0 1"
        self.assertEqual(sf.game_result("anti-losalamos", fen, []), sf.VALUE_NONE)
        self.assertTrue(sf.legal_moves("anti-losalamos", fen, []))

if __name__ == "__main__":
    unittest.main()
