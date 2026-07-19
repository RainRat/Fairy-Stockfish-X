import unittest
import subprocess
import sys
from pathlib import Path

ROOT_DIR = Path(__file__).resolve().parents[2]

import pyffish as sf

class TestBindings(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with open(ROOT_DIR / "src" / "variants.ini", "r", encoding="utf-8") as f:
            sf.load_variant_config(f.read())

    def test_is_capture_invalid_move(self):
        with self.assertRaises(ValueError):
            sf.is_capture("chess", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", [], "invalid")

    def test_validate_position_reports_encoding_failure(self):
        with self.assertRaises((ValueError, UnicodeEncodeError)):
            sf.validate_position("chess", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", ["\udcff"])

    def test_game_result_not_terminal(self):
        res = sf.game_result("chess", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", [])
        self.assertEqual(res, sf.VALUE_NONE)

    def test_parser_whitespace_and_inheritance_contract(self):
        sf.load_variant_config(
            "[api-two-boards:chess]\n"
            "twoBoards = true   \n"
            "[api-capture-hand:chess]\n"
            "captureType = hand   \n"
            "[api-spaced-parent:chess]\n"
            "twoBoards = true\n"
            "[api-spaced-child:api-spaced-parent]\n"
        )
        self.assertTrue(sf.two_boards("api-two-boards"))
        self.assertTrue(sf.captures_to_hand("api-capture-hand"))
        self.assertTrue(sf.two_boards("api-spaced-child"))

    def test_validation_returns_binding_status_for_parser_diagnostics(self):
        sf.load_variant_config(
            """
[api-counter-diagnostics:gothic]
maxRank = 8
maxFile = 8
checkCounting = true
startFen = 4k3/8/8/8/8/8/8/4K3 w - - 0 1
"""
        )
        self.assertEqual(
            sf.validate_fen(
                "4k3/8/8/8/8/8/8/4K3 w - - x 1 a0a0",
                "api-counter-diagnostics",
                False,
            ),
            -2,
        )
        self.assertEqual(
            sf.validate_fen(
                "4k3/8/8/8/8/8/8/4K3 w - - 0 x a0a0",
                "api-counter-diagnostics",
                False,
            ),
            -1,
        )

    def test_promotion_origin_validation(self):
        code = r'''
import pyffish

pyffish.load_variant_config(r"""
[api-promotion-origin:chess]
pieceDrops = true
captureType = hand
promotedPieceType = p:q n:q k:q
""")

assert pyffish.validate_fen(
    "4k3/8/8/8/8/8/8/Q~:N3K3[] w - - 0 1", "api-promotion-origin"
) == 1
assert pyffish.validate_fen(
    "4k3/8/8/8/8/8/8/Q~:n3K3[] w - - 0 1", "api-promotion-origin"
) != 1
assert pyffish.validate_fen(
    "4k3/8/8/8/8/8/8/Q~:K3K3[] w - - 0 1", "api-promotion-origin"
) == 1
'''
        subprocess.run([sys.executable, "-c", code], check=True)

    def test_validation_diagnostic_messages_are_exposed_by_the_binding(self):
        code = r'''
import pyffish

pyffish.load_variant_config(r"""
[api-castling-diagnostics:gothic]
castling = true
castlingKingFile = f
castlingKingsideFile = i
castlingQueensideFile = c
castlingRookKingsideFile = j
castlingRookQueensideFile = b
startFen = 10/10/10/10/10/10/10/1R3K3R w JQ - 0 1

[api-counter-diagnostics:gothic]
maxRank = 8
maxFile = 8
checkCounting = true
startFen = 4k3/8/8/8/8/8/8/4K3 w - - 0 1
""")

for fen, variant in [
    ("10/10/10/10/10/10/10/1R3K2R1 w JQ - 0 1", "api-castling-diagnostics"),
    ("10/10/10/10/10/10/10/1R3K6 w KQ - 0 1", "api-castling-diagnostics"),
    ("4k3/8/8/8/8/8/8/4K3 w - - x 1 a0a0", "api-counter-diagnostics"),
    ("4k3/8/8/8/8/8/8/4K3 w - - 0 x a0a0", "api-counter-diagnostics"),
]:
    print(f"validate_fen {variant} {pyffish.validate_fen(fen, variant, False)}")
'''
        result = subprocess.run(
            [sys.executable, "-c", code], capture_output=True, text=True, check=True
        )
        diagnostics = result.stdout + result.stderr
        self.assertIn("validate_fen api-castling-diagnostics -5", diagnostics)
        self.assertIn("validate_fen api-counter-diagnostics -2", diagnostics)
        self.assertIn("validate_fen api-counter-diagnostics -1", diagnostics)
        self.assertIn("Invalid half move counter: 'x'.", diagnostics)
        self.assertIn("Invalid move counter: 'x'.", diagnostics)


class TestPublicAPI(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with open(ROOT_DIR / "src" / "variants.ini", "r", encoding="utf-8") as f:
            sf.load_variant_config(f.read())

    def test_metadata_and_variant_listing(self):
        self.assertEqual(len(sf.version()), 3)
        self.assertTrue(sf.info().startswith("Fairy-Stockfish"))
        self.assertIn("chess", sf.variants())
        self.assertIn("shogun", sf.variants())
        self.assertIn("hostage", sf.variants())

    def test_option_and_variant_shape_contract(self):
        self.assertIsNone(sf.set_option("UCI_Variant", "capablanca"))
        self.assertFalse(sf.two_boards("chess"))
        self.assertTrue(sf.two_boards("bughouse"))
        self.assertFalse(sf.captures_to_hand("seirawan"))
        self.assertTrue(sf.captures_to_hand("shouse"))

    def test_option_and_start_fen(self):
        sf.set_option("Verbosity", 0)
        self.assertIn(" w ", sf.start_fen("chess"))
        self.assertEqual(
            sf.start_fen("capablanca"),
            "rnabqkbcnr/pppppppppp/10/10/10/10/PPPPPPPPPP/RNABQKBCNR w KQkq - 0 1",
        )
        self.assertEqual(
            sf.start_fen("xiangqi"),
            "rnbakabnr/9/1c5c1/p1p1p1p1p/9/9/P1P1P1P1P/1C5C1/9/RNBAKABNR w - - 0 1",
        )
        with self.assertRaisesRegex(ValueError, "No such variant"):
            sf.start_fen("this_variant_does_not_exist")

    def test_move_and_serialization_shapes(self):
        fen = sf.start_fen("chess")
        moves = sf.legal_moves("chess", fen, [])
        self.assertIsInstance(moves, list)
        self.assertIn("e2e4", moves)
        self.assertIsInstance(sf.get_fen("chess", fen, ["e2e4"]), str)
        self.assertIsInstance(sf.get_san("chess", fen, "e2e4"), str)

    def test_public_predicate_return_types(self):
        fen = sf.start_fen("chess")
        self.assertIsInstance(sf.is_capture("chess", fen, [], "e2e4"), bool)
        self.assertIsInstance(sf.get_san_moves("chess", fen, ["e2e4", "e7e5"]), list)
        self.assertIsInstance(sf.gives_check("chess", fen, ["e2e4"]), bool)
        self.assertIsInstance(sf.piece_to_partner("chess", fen, ["e2e4"]), str)
        self.assertIsInstance(sf.evaluate("chess", fen, []), int)
        self.assertEqual(sf.game_result("chess", fen, []), sf.VALUE_NONE)
        immediate = sf.is_immediate_game_end("chess", fen, [])
        optional = sf.is_optional_game_end("chess", fen, [])
        self.assertIsInstance(immediate, tuple)
        self.assertEqual(len(immediate), 2)
        self.assertIsInstance(immediate[0], bool)
        self.assertIsInstance(immediate[1], int)
        self.assertIsInstance(optional, tuple)
        self.assertEqual(len(optional), 2)
        self.assertIsInstance(optional[0], bool)
        self.assertIsInstance(optional[1], int)
        self.assertIsInstance(sf.has_insufficient_material("chess", fen, []), tuple)

    def test_validation_and_fog_are_binding_values(self):
        fen = sf.start_fen("chess")
        self.assertEqual(sf.validate_fen(fen, "chess"), 1)
        self.assertEqual(sf.validate_position("chess", fen, []), 1)
        self.assertIsInstance(sf.get_fog_fen(fen, "chess"), str)

if __name__ == "__main__":
    unittest.main()
