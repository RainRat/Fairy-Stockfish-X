import unittest
from pathlib import Path

ROOT_DIR = Path(__file__).resolve().parents[1]

import pyffish as sf

class TestBindings(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with open(ROOT_DIR / "src" / "variants.ini", "r", encoding="utf-8") as f:
            sf.load_variant_config(f.read())
        sf.load_variant_config(
            """
[goal-immediate:fairy]
nMoveRuleImmediate = 1
nMoveRule = 0
startFen = k3r3/8/8/8/8/8/8/4K3 w - - 2 1

[drop-immediate:fairy]
pieceDrops = true
dropPieceTypes = q
nMoveRuleImmediate = 1
nMoveRule = 0
startFen = 4k3/8/8/8/8/8/8/4K3[Q] w - - 1 1

[prison-no-king:fairy]
king = -
checking = false
prisonPawnPromotion = true
startFen = 8/8/8/8/8/8/4P3/8 w - - 0 1
"""
        )

    def test_is_capture_invalid_move(self):
        with self.assertRaises(ValueError):
            sf.is_capture("chess", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", [], "invalid")

    def test_validate_position_reports_encoding_failure(self):
        with self.assertRaises((ValueError, UnicodeEncodeError)):
            sf.validate_position("chess", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", ["\udcff"])

    def test_game_result_not_terminal(self):
        res = sf.game_result("chess", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", [])
        self.assertEqual(res, sf.VALUE_NONE)

    def test_enclosing_drop_startpos_not_drawn_by_insufficient_material(self):
        fen = sf.start_fen("snort")
        self.assertTrue(sf.legal_moves("snort", fen, []))
        self.assertEqual(sf.game_result("snort", fen, []), sf.VALUE_NONE)
        self.assertEqual(sf.has_insufficient_material("snort", fen, []), (False, False))

    def test_brandub_missing_king_is_not_draw(self):
        fen = "7/7/3r3/2r1r2/3R3/7/7 w - - 0 1"
        self.assertLess(sf.game_result("brandub", fen, []), 0)

    def test_antiminishogi_startpos_not_terminal(self):
        fen = "rbsgk/4p/5/P4/KGSBR[] w - - 0 1"
        self.assertEqual(sf.game_result("antiminishogi", fen, []), sf.VALUE_NONE)
        self.assertTrue(sf.legal_moves("antiminishogi", fen, []))

    def test_konane_opening_removals_are_not_rendered_as_passes(self):
        fen = "MmMmMmMmMm/mMmMmMmMmM/MmMmMmMmMm/mMmMmMmMmM/MmMmMmMmMm/mMmMmMmMmM/MmMmMmMmMm/mMmMmMmMmM/MmMmMmMmMm/mMmMmMmMmM w - - 0 1"
        moves = sf.legal_moves("konane", fen, [])
        self.assertTrue(moves)
        self.assertTrue(any(move != "0000" for move in moves))

    def test_anti_losalamos_missing_queen_not_terminal(self):
        fen = "rn1knr/pppppp/6/6/PPPPPP/RNQKNR w - - 0 1"
        self.assertEqual(sf.game_result("anti-losalamos", fen, []), sf.VALUE_NONE)
        self.assertTrue(sf.legal_moves("anti-losalamos", fen, []))

    def test_move_piece_self_move_promoted(self):
        # White has a promoted checkers king (K) on c3 and a man on e5.
        # Black has a checkers man on f6.
        # FEN: 8/8/5m2/4M3/8/2K5/8/7K b - - 0 1
        fen = "8/8/5m2/4M3/8/2K5/8/7K b - - 0 1"
        # Black plays f6d4 (jump capture), forcing White to pass (0000) on c3 where the King is.
        next_fen = sf.get_fen("checkers", fen, ["f6d4", "0000"])
        self.assertIn("K", next_fen)

    def test_immediate_n_move_rule_in_check_uses_non_recursive_legal_move_probe(self):
        is_end, result = sf.is_immediate_game_end("goal-immediate", "k3r3/8/8/8/8/8/8/4K3 w - - 2 1", [])
        self.assertTrue(is_end)
        self.assertEqual(result, sf.VALUE_DRAW)

    def test_drop_check_under_immediate_n_move_rule_is_not_misclassified_as_mate(self):
        next_fen = sf.get_fen("drop-immediate", "4k3/8/8/8/8/8/8/4K3[Q] w - - 1 1", ["Q@e7"])
        self.assertIn("4Q3", next_fen)
        self.assertIn(" b ", next_fen)

    def test_prison_pawn_promotion_without_opponent_king_is_safe(self):
        is_end, result = sf.is_immediate_game_end("prison-no-king", "8/8/8/8/8/8/4P3/8 w - - 0 1", [])
        self.assertFalse(is_end)

if __name__ == "__main__":
    unittest.main()
