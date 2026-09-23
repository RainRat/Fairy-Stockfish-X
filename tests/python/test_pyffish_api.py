import unittest
import json
import subprocess
import sys
from pathlib import Path

ROOT_DIR = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT_DIR))

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

    def test_move_list_rejects_invalid_move(self):
        # Whole-request contract: one bad token fails the call (contrast the
        # native UCI truncation documented in DEVELOPING.md).
        with self.assertRaisesRegex(ValueError, "Invalid move 'bogus'"):
            sf.game_result(
                "chess",
                "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
                ["e2e4", "bogus", "g1f3"],
            )

    def test_game_result_nonroyal_draw_threshold(self):
        sf.load_variant_config(
            "[api-nonroyal-draw:chess]\nnonRoyalDrawThreshold = 2\n"
        )
        # Quiet 1-vs-1 endgame is drawn ...
        res = sf.game_result(
            "api-nonroyal-draw", "4k3/8/8/8/8/8/8/R3K3 w - - 0 1", []
        )
        self.assertEqual(res, 0)
        # ... while 2-vs-1 plays on.
        res = sf.game_result(
            "api-nonroyal-draw", "4k3/8/8/8/8/8/8/RR2K3 w - - 0 1", []
        )
        self.assertEqual(res, sf.VALUE_NONE)

    def test_load_variant_config_reports_added_count(self):
        # Loading is add-only: redefinitions are skipped, never replaced.
        added = sf.load_variant_config(
            "[api-load-count:chess]\n"
            "startFen = 8/8/8/8/8/8/8/4K2k w - - 0 1\n"
        )
        self.assertEqual(added, 1)
        reloaded = sf.load_variant_config(
            "[api-load-count:chess]\n"
            "startFen = 8/8/8/8/8/8/8/4K2k w - - 0 1\n"
        )
        self.assertEqual(reloaded, 0)
        self.assertEqual(sf.load_variant_config(""), 0)

    def test_game_result_material_counting_armageddon(self):
        # The armageddon stalemate resolves decisively through engine-level
        # material counting, identically in every binding (not via the
        # binding-layer counting fallback).
        res = sf.game_result(
            "armageddon",
            "2Q2bnr/4p1pq/5pkr/7p/7P/4P3/PPPP1PP1/RNB1KBNR w KQ - 1 10",
            ["c8e6"],
        )
        self.assertEqual(res, sf.VALUE_MATE)

    def test_game_result_checkmate_is_draw(self):
        # Variants where checkmate itself is a draw must report a draw, not a
        # loss: there is no mate score to normalize to.
        sf.load_variant_config(
            "[api-matedraw-only:chess]\ncheckmateValue = draw\n"
        )
        res = sf.game_result(
            "api-matedraw-only",
            "rnb1kbnr/pppp1ppp/8/4p3/5PPq/8/PPPPP2P/RNBQKBNR w KQkq - 0 1",
            [],
        )
        self.assertEqual(res, 0)

    def test_game_result_material_counting_overrides_draw(self):
        # A drawn terminal (here checkmate-is-a-draw) still resolves through
        # material counting when the variant enables it, matching the
        # ffish.js/DLL result() fallback. Black draw odds decide for Black.
        sf.load_variant_config(
            "[api-matedraw-count:chess]\n"
            "checkmateValue = draw\n"
            "materialCounting = blackdrawodds\n"
        )
        res = sf.game_result(
            "api-matedraw-count",
            "rnb1kbnr/pppp1ppp/8/4p3/5PPq/8/PPPPP2P/RNBQKBNR w KQkq - 0 1",
            [],
        )
        self.assertEqual(res, -sf.VALUE_MATE)

    def test_validate_fen_gating_mask_suffix(self):
        good = (
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR[HEhe]"
            " w KQkq|11111111/11111111 - 0 1"
        )
        self.assertEqual(sf.validate_fen(good, "seirawan", False), 1)
        for bad in (
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR[HEhe]"
            " w KQkq|garbage - 0 1",
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR[HEhe]"
            " w KQkq|1111111/11111111 - 0 1",
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR[HEhe]"
            " w KQkq|11111111 - 0 1",
        ):
            self.assertEqual(sf.validate_fen(bad, "seirawan", False), -5)

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

    def test_fen_bounds_and_side_to_move_validation(self):
        with open(ROOT_DIR / "src" / "variants.ini", "r", encoding="utf-8") as f:
            sf.load_variant_config(f.read())

        chess_fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR"
        self.assertEqual(
            sf.validate_fen(f"{chess_fen} ww KQkq - 0 1", "chess", False),
            -6,
        )
        self.assertEqual(
            sf.validate_fen(f"{chess_fen} w KQkq a9 0 1", "chess", False),
            -4,
        )
        self.assertEqual(
            sf.validate_fen(
                "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/9 w KQkq - 0 1",
                "chess",
                False,
            ),
            -8,
        )
        self.assertEqual(
            sf.validate_fen(
                "4k3/8/8/8/8/8/8/4K3[999999999999999999999999P] w - - 0 1",
                "crazyhouse",
                False,
            ),
            -7,
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

    def test_variant_info(self):
        info = json.loads(sf.variant_info("chess"))
        self.assertEqual(info["schemaVersion"], 2)
        self.assertEqual(info["name"], "chess")
        self.assertEqual(info["board"]["width"], 8)
        self.assertEqual(info["board"]["height"], 8)
        self.assertEqual(info["board"]["startFen"], sf.start_fen("chess"))
        self.assertEqual([piece["fen"]["white"] for piece in info["pieces"]], ["P", "N", "B", "R", "Q", "K"])
        self.assertEqual(info["gameEnd"]["kingType"], "king")
        self.assertEqual(info["royalPieceTypes"], ["king"])
        self.assertFalse(info["castling"]["wins"]["white"]["kingSide"])
        self.assertIsInstance(info["protocol"]["pieceToCharTable"], str)
        self.assertNotIn("pieceTypesByFile", info["promotion"])
        self.assertNotIn("pieceTypesByRank", info["promotion"])
        self.assertEqual(info["promotion"]["promotedPieceTypes"], {})
        self.assertEqual(info["promotion"]["captureDemotedPieceTypes"], {})

        janggi_info = json.loads(sf.variant_info("janggi"))
        self.assertIn("e2", janggi_info["board"]["diagonalLines"])
        self.assertEqual(janggi_info["movement"]["soldierPromotionRank"], 1)

        sf.load_variant_config("[variantinfocustomroyal:chess]\nking = k:KN\n")
        custom_royal = json.loads(sf.variant_info("variantinfocustomroyal"))
        king = next(piece for piece in custom_royal["pieces"] if piece["type"] == "king")
        self.assertEqual(king["customBetza"], "KN")

        sf.load_variant_config(
            "[variantinfocomposable:chess]\n"
            "freezePieceTypes = b\n"
            "freezeImmunePieceTypes = p\n"
            "freezeDiagonals = false\n"
            "trapRegion = d4\n"
            "trapProtection = friendly-orthogonal\n"
        )
        composable = json.loads(sf.variant_info("variantinfocomposable"))
        self.assertEqual(composable["movement"]["freezePieceTypes"], ["bishop"])
        self.assertEqual(composable["movement"]["freezeImmunePieceTypes"], ["pawn"])
        self.assertFalse(composable["movement"]["freezeDiagonals"])
        self.assertEqual(composable["capture"]["trapRegion"], ["d4"])
        self.assertEqual(composable["capture"]["trapProtection"], "friendly-orthogonal")

        sf.load_variant_config(
            "[variantinfodrops:chess]\n"
            "dropNoCheckmate = n\n"
            "dropOppositeColoredBishop = true\n"
        )
        drops = json.loads(sf.variant_info("variantinfodrops"))
        self.assertEqual(drops["gameEnd"]["noCheckmateTypes"], {"white": ["knight"], "black": ["knight"]})
        self.assertEqual(drops["drops"]["oppositeColorTypes"], {"white": ["bishop"], "black": ["bishop"]})

        sf.load_variant_config(
            "[variantinfotwostep:chess]\n"
            "customPiece1 = l:ADNWK\n"
            "twoStepMoves = l:*\n"
        )
        twostep = json.loads(sf.variant_info("variantinfotwostep"))
        self.assertEqual(twostep["movement"]["twoStepMoves"], {"custom1": "*"})
        self.assertEqual(json.loads(sf.variant_info("chess"))["movement"]["twoStepMoves"], {})

        sf.load_variant_config(
            "[variantinfohook:chess]\n"
            "customPiece1 = h:0\n"
            "hookMoves = h:R-sR:2\n"
        )
        hook = json.loads(sf.variant_info("variantinfohook"))
        self.assertEqual(hook["movement"]["hookMoves"],
                         {"custom1": {"pairs": "N>E,N>W,S>E,S>W",
                                      "firstRange": 0, "secondRange": 0, "captureLimit": 2}})
        self.assertEqual(json.loads(sf.variant_info("chess"))["movement"]["hookMoves"], {})

        with self.assertRaisesRegex(ValueError, "Unknown variant"):
            sf.variant_info("does-not-exist")

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
