
import pyffish as sf
import unittest
from pathlib import Path

ROOT_DIR = Path(__file__).resolve().parents[1]

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

if __name__ == "__main__":
    unittest.main()
