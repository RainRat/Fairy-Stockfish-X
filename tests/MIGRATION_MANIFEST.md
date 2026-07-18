# Test organization migration manifest

This manifest records the native tests moved out of the Python binding and the
Python rule assertions converted to native or UCI coverage. It is retained as
a stable coverage map so future changes can identify the owning suite without
reconstructing the old test history.

| Previous entry | Destination |
| --- | --- |
| `pyffish_runCppTests`: promotion-square color handling | native `promotion` |
| `pyffish_runCppTests`: empty clone target | native `movement` |
| `pyffish_runCppTests`: castling and synthetic occupancy | native `occupancy` |
| `pyffish_runCppTests`: paired-drop pawn key | native `state` |
| `pyffish_runCppTests`: simulated castling, en passant, rifle, blast, clone, gating, paired placement, and walls | native `occupancy` |
| `pyffish_runCppTests`: laser key/material/FEN and do/undo cases | native `state` |
| `pyffish_runCppTests`: quiet-check laser cases | native `royal` |
| Python adjudication/missing-king rule probes | native `adjudication` |
| `dots-boxes-2x2` result and pass-move matrix from `board-game-smoke.sh` | native `board-games` |
| `TestBindings` from `tests/test_binding_regression.py` | `tests/python/test_pyffish_api.py` |
| parser whitespace/inheritance and validation diagnostics | `tests/python/test_pyffish_api.py` binding-contract tests |
| `TestPyffish.test_run_cpp_tests` and its inline variants | native groups above |
| `TestRoyalCaptureNoKings.test_royal_capture` | `royal-legality` UCI capture section |
| `stationary-castling.sh` | `royal-legality` |
| CI immobility-illegal hopper block | `movement` |
| `test_strong_pawn_basics`, `test_alterga_basics`, `test_royal_piece_no_through_check`, `test_asymmetric_walling_turns`, `test_drop_king_last`, `test_paired_drop_points`, `test_ichess_setup_basics` | `movement` UCI rule-matrix case |
| `test_feature_combo_regressions`, `test_capture_anything_knight_self_capture`, `test_capture_anything_pawn_self_capture_resets_clock`, `test_self_capture_hand_keeps_mover_color`, `test_benedict_morph_capture_changes_piece_type`, `test_benedict_morph_king_stays_king`, `test_blast_on_capture_mover_center` | `captures-effects` UCI rule-matrix case |
| `test_enclosing_drop_startpos_not_drawn_by_insufficient_material`, `test_brandub_missing_king_is_not_draw`, `test_antiminishogi_startpos_not_terminal`, `test_anti_losalamos_missing_queen_not_terminal`, `test_immediate_n_move_rule_in_check_uses_non_recursive_legal_move_probe`, `test_type_goal_and_region_simultaneous_connection_uses_simultaneous_policy`, `test_prison_pawn_promotion_without_opponent_king_is_safe` | native `adjudication` |
| `test_konane_opening_removals_are_not_rendered_as_passes`, `test_move_piece_self_move_promoted` | `movement` UCI rule-matrix case |
| `test_drop_check_under_immediate_n_move_rule_is_not_misclassified_as_mate` | `promotion-drops` UCI rule-matrix case |
| `special-regressions.sh`, `fast-regression-rules.sh`, `local-regression-inline.sh`, `unorthodox-interactions.sh` | renamed suite-owned capture interaction cases; assertions remain registered under `captures-effects` |
| `misc-engine-regressions.sh` | renamed suite-owned engine/search case under `search-evaluation` |
| `all-variants-smoke.sh`, `new-variants-smoke.sh`, `fast-variant-regressions.sh`, `mini-variant-regressions.sh` | renamed suite-owned variant loading/rule matrix cases |
| `dots-and-boxes.sh`, `hindustani-baseline.sh`, `largeboard-seirawan.sh`, `villagers.sh`, `vlb-regressions.sh`, `wrapped-connect-win.sh`, `wrapped-topology-smoke.sh` | renamed suite-owned variant smoke, gating, topology, and goal cases |

### Shell case ownership

All assertion-bearing historical shell fragments are now below one of the ten
suite directories. They are implementation fragments, not standalone test
entry points; the runner resolves them only through the owning suite. The
inventory below records each relocated source name and destination suite:

| Relocated source name | Suite |
| --- | --- |
| `blast-pattern.sh`, `bycatch-undo-parity.sh`, `capture-options-regressions.sh`, `color-change-variants.sh` | captures-effects |
| `capture-rule-definitions.sh`, `jump-capture-effects.sh`, `cross-feature-state.sh`, `petrify-transfer.sh` | captures-effects |
| `pulling.sh`, `rifle-chess.sh`, `capture-effects-special.sh`, `swapping.sh`, `capture-interactions.sh`, `rule-matrix-captures.sh` | captures-effects |
| `explicit-custom-piece-replacements.sh`, `incomplete-baselines.sh`, `parser-regressions.sh` | config |
| `fast-regression-piece-regions.sh`, `gadsden-toroidal.sh`, `geometry-regressions.sh`, `movegen-regressions.sh` | movement |
| `non-knight-riders.sh`, `rider-regressions.sh`, `separate-realms.sh`, `ski-sliders.sh` | movement |
| `test_hex_boards.sh`, `universal-hopper.sh`, `wrapping-topology.sh`, `rule-matrix-movement.sh` | movement |
| `fairy-notation-regressions.sh`, `protocol.sh`, `setup-chess.sh`, `xboard-regressions.sh` | notation-protocol |
| `capture-promotion-regressions.sh`, `castling-promoted-piece.sh`, `chained-piece-promotion.sh`, `drop-regressions.sh` | promotion-drops |
| `piece-promotion-gating.sh`, `promotion-require-in-prison.sh`, `shogi-pawn-drop-mate-split.sh`, `wrapping-promotion-movegen.sh`, `rule-matrix-drops.sh` | promotion-drops |
| `blast-legal-regressions.sh`, `ep-pseudoroyal-regressions.sh`, `gating-check-regression.sh`, `kings-or-lemmings.sh` | royal-legality |
| `pseudoroyal-capture-illegal.sh`, `quiet-check-special-moves.sh`, `royal-variant-regressions.sh`, `stationary-castling.sh`, `test_extinction.sh` | royal-legality |
| `bench-regressions.sh`, `eval-geometry-regressions.sh`, `kxk-fairy-endgames.sh`, `engine-search-regressions.sh` | search-evaluation |
| `nnue-affine-regression.sh`, `nnue-export-failure.sh`, `nnue-variant-dimension-guard.sh`, `non8x8-endgames.sh`, `reprosearch.sh` | search-evaluation |
| `spell-freeze-regressions.sh`, `spell-potion-movegen.sh` | spells |
| `clone-firstmove-split.sh`, `concurrent-variant-magics.sh`, `in-place-transform-undo.sh`, `multimove-rule50.sh` | state-transitions |
| `piece-type-bitboard-regressions.sh`, `state-sync-key.sh`, `stateinfo-regressions.sh`, `touched-search-regressions.sh` | state-transitions |
| `variant-load-all.sh`, `board-game-smoke.sh`, `variant-rules-matrix.sh`, `variant-promotion-baselines.sh` | variants-smoke |
| `gating-large-board.sh`, `small-variant-rules.sh`, `variant-load-matrix.sh`, `royal-pawn-variants.sh` | variants-smoke |
| `very-large-board-regressions.sh`, `connect-goals.sh`, `topology-smoke.sh` | variants-smoke |

The former Python rule methods have these owning destinations. Their assertions
now run in the listed native or UCI suites; only binding-shape checks remain in
`tests/python/test_pyffish_api.py`.

| Archived method | Owning suite |
| --- | --- |
| `test_version`, `test_info`, `test_variants_loaded`, `test_set_option` | config/API representative |
| `test_duplicate_variant_warnings_are_summarized`, `test_piece_points_clamping_warnings` | config |
| `test_two_boards`, `test_captures_to_hand`, `test_start_fen` | config / variants-smoke |
| `test_legal_moves`, `test_strong_pawn_basics`, `test_alterga_basics` | movement |
| `test_feature_combo_regressions` | captures-effects / state-transitions |
| `test_castling`, `test_royal_piece_no_through_check` | royal-legality |
| `test_asymmetric_walling_turns`, `test_witch_hunting_basics` | movement / royal-legality |
| `test_drop_king_last`, `test_paired_drop_points` | promotion-drops |
| `test_liberty_capture_actions`, `test_capture_anything_knight_self_capture`, `test_capture_anything_pawn_self_capture_resets_clock` | captures-effects |
| `test_ichess_setup_basics`, `test_chesscom_custom_setups_basics` | variants-smoke |
| `test_checkers_jump_and_promotion`, `test_whaleshogi_dolphin_promotion_cycle` | promotion-drops |
| `test_standard_fairy_riders_and_ski_sliders` | movement |
| `test_get_fen`, `test_get_san`, `test_get_san_moves`, `test_gives_check`, `test_is_capture` | notation-protocol |
| `test_self_capture_hand_keeps_mover_color`, `test_benedict_morph_capture_changes_piece_type`, `test_benedict_morph_king_stays_king` | captures-effects |
| `test_spell_chess_jump_capture_wins_immediately`, `test_spell_chess_freeze_check_does_not_win`, `test_spell_chess_frozen_rook_blocks_castling`, `test_spell_chess_castling_through_attack_requires_freeze` | spells |
| `test_spell_chess_cannot_castle_out_of_check_without_freeze`, `test_spell_chess_jump_potion_does_not_bypass_castling_blockers`, `test_spell_chess_frozen_pawn_cannot_capture_en_passant` | spells / royal-legality |
| `test_spell_chess_potion_fen_extension_roundtrip`, `test_spell_chess_potion_fen_extension_parse`, `test_spell_chess_potion_fen_roundtrip_after_both_potion_types` | spells / notation-protocol |
| `test_piece_to_partner` | notation-protocol |
| `test_game_result`, `test_pseudoroyal_drop_cannot_land_in_check`, `test_runtime_royal_self_capture_is_illegal` | royal-legality |
| `test_runtime_royal_no_through_check_uses_actual_royal`, `test_pseudoroyal_loss_waits_for_candidate_types_to_disappear`, `test_extinction_value_uses_extinct_side` | royal-legality |
| `test_is_immediate_game_end`, `test_racing_kings_goal_adjudication`, `test_loa_simultaneous_and_opponent_connection` | royal-legality / native adjudication |
| `test_connect_goal_simul_value_by_mover`, `test_connection_all_remaining_pieces`, `test_toroidal_line_counting_fix` | royal-legality / variants-smoke |
| `test_is_optional_game_end`, `test_has_insufficient_material` | royal-legality |
| `test_validate_fen`, `test_validate_position`, `test_validate_fen_promoted_pieces`, `test_get_fog_fen` | config / notation-protocol |
| `test_blast_on_capture_mover_center` | captures-effects |
| `test_evaluate`, `test_racing_kings_endgame_eval`, `test_atomic_endgame_eval` | search-evaluation |
| `test_push_state_consistency`, `test_magic_geometry_pollution` | state-transitions |
| `test_spell_chess` | spells |
| `test_laser_variants` | state-transitions / movement |

The former mixed `test.py` entry point is now a compatibility launcher for the
public binding contract. The public binding representatives are in
`tests/python/test_pyffish_api.py`; pure rule assertions are owned by the
native and UCI destinations above.

The old C++ entry point and Python caller are removed only after the native
harness is run against the matching board-family objects.
