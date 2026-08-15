import sys
import unittest
from pathlib import Path


TOOLS = Path(__file__).resolve().parents[2] / "tools"
sys.path.insert(0, str(TOOLS))

from arimaa_tournament import (  # noqa: E402
    ArimaaState,
    STANDARD_SETUP_FEN,
    TournamentError,
    fsx_position_command,
    parse_aei_move,
    parse_fsx_move,
)


class ArimaaTournamentTests(unittest.TestCase):
    def test_standard_setup_is_boundary_position(self):
        state = ArimaaState.standard_setup()
        self.assertEqual(state.fen(), STANDARD_SETUP_FEN)
        self.assertEqual(len(state.board.board), 32)
        self.assertEqual(state.board.side, "g")

    def test_active_position_complete_turn_count(self):
        state = ArimaaState.from_fen(
            "cdhmehdc/rrrrrrrr/8/8/8/8/RRRRRRRR/CDHMEHDC w - - 0 1"
        )
        self.assertEqual(state.legal_turn_count(), 20652)

    def test_normal_aei_turn_and_fsx_encoding(self):
        state = ArimaaState.from_fen("8/8/8/8/8/8/R7/8 w - - 0 1")
        result = state.apply_turn(parse_aei_move("Ra2n"))
        self.assertEqual(result.aei(), "Ra2n")
        self.assertEqual(result.fsx(), "a2a3")
        self.assertEqual(result.state.board.board["a3"], "R")
        self.assertEqual(result.state.board.side, "s")

    def test_pull_is_atomic_and_round_trips_fsx_notation(self):
        state = ArimaaState.from_fen("8/8/8/8/8/8/8/3Er3 w - - 0 1")
        result = state.apply_turn(parse_aei_move("Ed1n re1w"))
        self.assertEqual(result.fsx(), "d1d2,e1")

        translated = parse_fsx_move(state, "d1d2,e1")
        self.assertEqual([step.aei() for step in translated], ["Ed1n", "re1w"])
        self.assertEqual(state.apply_turn(translated).state.board.board, result.state.board.board)

    def test_push_is_atomic_and_round_trips_fsx_notation(self):
        state = ArimaaState.from_fen("8/8/8/8/8/8/8/3Er3 w - - 0 1")
        result = state.apply_turn(parse_aei_move("re1e Ed1e"))
        self.assertEqual(result.fsx(), "d1e1,f1")

        translated = parse_fsx_move(state, "d1e1,f1")
        self.assertEqual([step.aei() for step in translated], ["re1e", "Ed1e"])
        self.assertEqual(state.apply_turn(translated).state.board.board, result.state.board.board)

    def test_unprotected_trap_removes_piece(self):
        state = ArimaaState.from_fen("8/8/8/8/8/8/2R5/8 w - - 0 1")
        result = state.apply_turn(parse_aei_move("Rc2n"))
        self.assertNotIn("c3", result.state.board.board)

    def test_akimot_trap_annotation_is_not_a_physical_step(self):
        state = ArimaaState.from_fen("8/8/8/8/8/8/2R5/8 w - - 0 1")
        result = state.apply_turn(parse_aei_move("Rc2n Rc3x"))
        self.assertNotIn("c3", result.state.board.board)

    def test_friendly_support_preserves_trap_piece(self):
        state = ArimaaState.from_fen("8/8/8/8/8/1R6/2R5/8 w - - 0 1")
        result = state.apply_turn(parse_aei_move("Rc2n"))
        self.assertEqual(result.state.board.board.get("c3"), "R")

    def test_five_physical_steps_are_rejected(self):
        state = ArimaaState.from_fen("8/8/8/8/8/8/RR6/8 w - - 0 1")
        with self.assertRaises(TournamentError):
            state.apply_turn(parse_aei_move("Ra2n Ra3n Rb2n Rb3n Ra4n"))

    def test_whole_turn_pass_is_rejected(self):
        state = ArimaaState.from_fen("8/8/8/8/8/8/R7/8 w - - 0 1")
        with self.assertRaises(TournamentError):
            state.apply_turn(parse_aei_move("Ra2n Ra3s"))

    def test_intermediate_return_can_be_followed_by_real_step(self):
        state = ArimaaState.from_fen("8/8/8/8/8/8/R1R5/8 w - - 0 1")
        result = state.apply_turn(parse_aei_move("Ra2e Rb2w Ra2n"))
        self.assertEqual(result.state.board.board.get("a3"), "R")
        self.assertEqual(result.state.board.board.get("c2"), "R")

    def test_intermediate_return_cannot_end_turn(self):
        state = ArimaaState.from_fen("8/8/8/8/8/8/R1R5/8 w - - 0 1")
        with self.assertRaises(TournamentError):
            state.apply_turn(parse_aei_move("Ra2e Rb2w"))

    def test_frozen_piece_cannot_move(self):
        state = ArimaaState.from_fen("8/8/8/8/8/3Re3/8/8 w - - 0 1")
        with self.assertRaises(TournamentError):
            state.apply_turn(parse_aei_move("Rd3n"))

    def test_no_move_is_detected_for_a_frozen_side(self):
        state = ArimaaState.from_fen("8/8/8/8/8/3Re3/8/8 w - - 0 1")
        self.assertFalse(state.has_legal_turn())

    def test_fsx_position_replays_history_for_repetition(self):
        state = ArimaaState.from_fen("8/8/8/8/8/8/R6r/8 w - - 0 1")
        for turn in ("Ra2e", "rh2w", "Rb2w", "rg2e",
                     "Ra2e", "rh2w", "Rb2w"):
            state = state.apply_turn(parse_aei_move(turn)).state

        command = fsx_position_command(state)
        self.assertIn("position fen 8/8/8/8/8/8/R6r/8 w - - 0 1 moves ", command)
        self.assertEqual(command.count("a2b2"), 2)
        self.assertEqual(command.count("h2g2"), 2)


if __name__ == "__main__":
    unittest.main()
