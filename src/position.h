/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2022 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef POSITION_H_INCLUDED
#define POSITION_H_INCLUDED

#include <cassert>
#include <cmath>
#include <cstdint>
#include <deque>
#include <memory> // For std::unique_ptr
#include <string>
#include <functional>

#include "bitboard.h"
#include "evaluate.h"
#include "psqt.h"
#include "types.h"
#include "variant.h"
#include "movegen.h"
#include "piece.h"

#include "nnue/nnue_accumulator.h"

namespace Stockfish {

extern Square JumpMidpoint[SQUARE_NB][SQUARE_NB];

struct SpellContext {
  Bitboard freezeExtra = Bitboard(0);
  Bitboard jumpRemoved = Bitboard(0);

  SpellContext() = default;
  SpellContext(Bitboard freezeExtra_, Bitboard jumpRemoved_)
      : freezeExtra(freezeExtra_), jumpRemoved(jumpRemoved_) {}

  bool active() const { return bool(freezeExtra | jumpRemoved); }
};

const SpellContext* current_spell_context() noexcept;
void set_current_spell_context(const SpellContext* ctx) noexcept;

struct ScopedSpellContext {
  SpellContext prev;
  bool prevActive;
  SpellContext ctx;
  bool active;

  ScopedSpellContext(Bitboard freezeExtra, Bitboard jumpRemoved)
      : prev(current_spell_context() ? *current_spell_context() : SpellContext()),
        prevActive(current_spell_context() && current_spell_context()->active()),
        ctx(freezeExtra, jumpRemoved),
        active(ctx.active()) {
    if (active)
      set_current_spell_context(&ctx);
  }

  ~ScopedSpellContext() {
    if (active)
      set_current_spell_context(prevActive ? &prev : nullptr);
  }
};

struct ReversiblePieceState {
  Piece piece = NO_PIECE;
  Piece unpromoted = NO_PIECE;
  bool promoted = false;

  void clear() {
    piece = NO_PIECE;
    unpromoted = NO_PIECE;
    promoted = false;
  }

  void set(Piece pc, bool isPromoted, Piece unpromotedPc = NO_PIECE) {
    piece = pc;
    promoted = isPromoted;
    unpromoted = isPromoted ? unpromotedPc : NO_PIECE;
  }

  explicit operator bool() const { return piece != NO_PIECE; }
};

/// StateInfo struct stores information needed to restore a Position object to
/// its previous state when we retract a move. Whenever a move is made on the
/// board (by calling Position::do_move), a StateInfo object must be passed.

struct StateInfo {

  // Copied when making a move
  Key    pawnKey;
  Key    materialKey;
  Value  nonPawnMaterial[COLOR_NB];
  int    castlingRights;
  int    rule50;
  int    pliesFromNull;
  int    countingPly;
  int    countingLimit;
  int    pointsCount[COLOR_NB];
  CheckCount checksRemaining[COLOR_NB];
  Bitboard epSquares;
  Square castlingKingSquare[COLOR_NB];
  Bitboard wallSquares;
  Bitboard deadSquares;
  Bitboard gatesBB[COLOR_NB];
  Bitboard not_moved_pieces[COLOR_NB];
  Bitboard potionZones[COLOR_NB][Variant::POTION_TYPE_NB];
  int potionCooldown[COLOR_NB][Variant::POTION_TYPE_NB];
  Key layoutKey;

  // Not copied when making a move (will be recomputed anyhow)
  Key        key;
  Key        boardKey;
  Bitboard   checkersBB;
  // Fairy-Stockfish-X split: broad royal-danger state, including pseudo-/anti-royal
  // bookkeeping, must not be conflated with actual "must evade now" check state.
  Bitboard   evasionCheckersBB;
  Piece      unpromotedBycatch[SQUARE_NB];
  Bitboard   bycatchSquares;
  Bitboard   promotedBycatch;
  Bitboard   demotedBycatch;
  Bitboard   blastPromotedSquares;
  StateInfo* previous;
  Bitboard   blockersForKing[COLOR_NB];
  Bitboard   pinners[COLOR_NB];
  Bitboard   checkSquares[PIECE_TYPE_NB];
  ReversiblePieceState captured;
  Square     captureSquare; // when != to_sq, e.g., en passant
  ReversiblePieceState dead;
  Piece      promotionPawn;
  Piece      consumedPromotionHandPiece;
  Bitboard   nonSlidingRiders;
  Bitboard   flippedPieces;
  Bitboard   pseudoRoyalCandidates;
  Bitboard   pseudoRoyals;
  PieceSet   extinctionSeen[COLOR_NB];
  OptBool    legalCapture;
  OptBool    legalEnPassant;
  Bitboard   chased;
  Bitboard   claimedSquares;
  Square     forcedJumpSquare;
  Move       move;
  Color      dropHandColor;
  int        forcedJumpStep;
  int        repetition;
  int        boardRepetition;
  PieceType removedGatingType;
  PieceType removedCastlingGatingType;
  PieceType capturedGatingType;
  Piece morphedFrom;
  Square morphSquare;
  ReversiblePieceState colorChanged;
  Square colorChangeSquare;
  Square pushTailSquare;
  int pushStepF;
  int pushStepR;
  int pushCount;
  int pushSnapshotCount;
  Square pushSnapshotSquares[32];
  Piece pushSnapshotPieces[32];
  Piece pushSnapshotUnpromoted[32];
  uint32_t pushSnapshotPromoted;
  int pushTransferCount;
  Piece pushTransferPieces[32];
  Piece pushTransferUnpromoted[32];
  uint32_t pushTransferPromoted;
  Square pullFromSquare;
  ReversiblePieceState pulled;
  bool       suppressedCaptureTransfer;
  bool       shak;
  bool       bikjang;
  bool       pass;
  bool       pendingClaimPass;
  bool       forcedJumpHasFollowup;
  bool       didMorph;
  bool       didColorChange;
  bool       didPush;
  bool       didPull;
  bool       pushStepwise;
  bool       pushEjected;
  bool       pushBlockedCapture;
  bool nnueRefreshNeeded;

  // Used by NNUE
  Eval::NNUE::Accumulator accumulator;
  DirtyPiece dirtyPiece;
};


/// A list to keep track of the position states along the setup moves (from the
/// start position to the position just before the search starts). Needed by
/// 'draw by repetition' detection. Use a std::deque because pointers to
/// elements are not invalidated upon list resizing.
typedef std::unique_ptr<std::deque<StateInfo>> StateListPtr;


/// Position class stores information regarding the board representation as
/// pieces, side to move, hash keys, castling info, etc. Important methods are
/// do_move() and undo_move(), used by the search to update node info when
/// traversing the search tree.
class Thread;

class Position {
public:
  static void init();

  Position() = default;
  Position(const Position&) = delete;
  Position& operator=(const Position&) = delete;

  // FEN string input/output
  Position& set(const Variant* v, const std::string& fenStr, bool isChess960, StateInfo* si, Thread* th, bool sfen = false);
  Position& set(const std::string& code, Color c, StateInfo* si);
  std::string fen(bool sfen = false, bool showPromoted = false, int countStarted = 0, std::string holdings = "-", Bitboard fogArea = 0) const;

  // Variant rule properties
  const Variant* variant() const;
  Rank max_rank() const;
  File max_file() const;
  int ranks() const;
  int files() const;
  bool two_boards() const;
  Bitboard board_bb() const;
  Bitboard dead_squares() const;
  Bitboard board_bb(Color c, PieceType pt) const;
  PieceSet piece_types() const;
  const std::string& piece_to_char() const;
  const std::string& piece_to_char_synonyms() const;
  const std::string& piece_symbol(Piece pc) const;
  const std::string& piece_symbol_synonym(Piece pc) const;
  Piece piece_from_symbol(const std::string& token) const;
  PieceType piece_type_from_symbol(const std::string& token) const;
  Bitboard promotion_zone(Color c) const;
  Bitboard promotion_zone(Color c, PieceType pt) const;
  Bitboard promotion_zone(Piece p) const;
  Bitboard mandatory_promotion_zone(Color c) const;
  Bitboard mandatory_promotion_zone(Color c, PieceType pt) const;
  Bitboard mandatory_promotion_zone(Piece p) const;
  Square promotion_square(Color c, Square s) const;
  PieceType main_promotion_pawn_type(Color c) const;
  PieceSet promotion_piece_types(Color c) const;
  PieceSet promotion_piece_types(Color c, Square s) const;
  bool sittuyin_promotion() const;
  int promotion_limit(PieceType pt) const;
  bool promotion_allowed(Color c, PieceType pt) const;
  bool promotion_allowed(Color c, PieceType pt, Square s) const;
  PieceType promoted_piece_type(PieceType pt) const;
  bool piece_promotion_on_capture() const;
  bool mandatory_pawn_promotion() const;
  bool mandatory_piece_promotion() const;
  bool piece_demotion() const;
  bool blast_on_capture() const;
  bool blast_on_capture(Piece mover, Piece captured) const;
  bool blast_on_capture(Move m) const;
  bool blast_on_move() const;
  bool blast_on_self_destruct() const;
  bool blast_promotion() const;
  bool blast_diagonals() const;
  bool blast_orthogonals() const;
  bool blast_center() const;
  bool zero_range_blast_on_capture(Piece mover, Piece captured) const;
  bool zero_range_blast_on_capture(Move m) const;
  PieceSet blast_immune_types() const;
  PieceSet death_on_capture_types() const;
  Bitboard blast_immune_bb() const;
  Bitboard blast_pattern(Square to) const;
  Bitboard blast_squares(Square to) const;
  int remove_connect_n() const;
  bool remove_connect_n_by_type() const;
  PieceSet mutually_immune_types() const;
  bool surround_capture_opposite() const;
  bool surround_capture_intervene() const;
  bool surround_capture_edge() const;
  Bitboard surround_capture_max_region() const;
  Bitboard surround_capture_hostile_region() const;
  EndgameEval endgame_eval() const;
  Bitboard double_step_region(Color c) const;
  Bitboard double_step_region(Color c, PieceType pt) const;
  Bitboard double_step_region(Piece p) const;
  Bitboard triple_step_region(Color c) const;
  Bitboard triple_step_region(Color c, PieceType pt) const;
  Bitboard triple_step_region(Piece p) const;
  bool castling_enabled() const;
  bool castling_dropped_piece() const;
  File castling_kingside_file() const;
  File castling_queenside_file() const;
  Rank castling_rank(Color c) const;
  File castling_king_file() const;
  PieceType castling_king_piece(Color c) const;
  PieceSet castling_rook_pieces(Color c) const;
  PieceType king_type() const;
  PieceType nnue_king() const;
  Square nnue_king_square(Color c) const;
  bool nnue_use_pockets() const;
  bool nnue_applicable() const;
  int nnue_piece_square_index(Color perspective, Piece pc) const;
  int nnue_piece_hand_index(Color perspective, Piece pc) const;
  int nnue_king_square_index(Square ksq) const;
  int nnue_wall_index_base() const;
  int nnue_points_index_base() const;
  int nnue_points_score_planes() const;
  int nnue_points_check_planes() const;
  int nnue_potion_zone_index_base() const;
  int nnue_potion_cooldown_index_base() const;
  bool free_drops() const;
  bool fast_attacks() const;
  bool fast_attacks2() const;
  bool wraps_files() const;
  bool wraps_ranks() const;
  bool topology_wraps() const;
  bool is_hex_board() const;
  bool checking_permitted() const;
  bool allow_checks() const;
  bool drop_checks() const;
  bool drop_mates() const;
  bool shogi_pawn_drop_mate_illegal() const;
  bool shogi_pawn_drop_mate_illegal(Color c) const;
  bool self_capture() const;
  bool self_capture(PieceType pt) const;
  bool rifle_capture() const;
  bool rifle_capture(Piece pc) const;
  bool rifle_capture(Move m) const;
  int pushing_strength(PieceType pt) const;
  bool has_pushing() const;
  int pulling_strength(PieceType pt) const;
  bool has_pulling() const;
  PieceSet adjacent_swap_move_types() const;
  bool has_adjacent_swapping() const;
  bool adjacent_swap_requires_empty_neighbor() const;
  bool swap_no_immediate_return() const;
  int swap_forbidden_plies() const;
  PushFirstColor push_first_color() const;
  PushRemoval pushing_removes() const;
  bool push_chain_enemy_only() const;
  bool push_capture_against_friendly_blocker() const;
  bool push_no_immediate_return() const;
  PieceSet edge_insert_types() const;
  bool edge_insert_only() const;
  Bitboard edge_insert_region(Color c) const;
  bool edge_insert_from_top(Color c) const;
  bool edge_insert_from_bottom(Color c) const;
  bool edge_insert_from_left(Color c) const;
  bool edge_insert_from_right(Color c) const;
  bool capture_morph() const;
  bool rex_exclusive_morph() const;
  bool must_capture() const;
  bool must_capture_en_passant() const;
  bool has_capture() const;
  bool has_en_passant_capture() const;
  bool must_drop() const;
  PieceType must_drop_type() const;
  bool opening_self_removal() const;
  bool in_opening_self_removal_phase() const;
  Bitboard opening_self_removal_targets(Color c) const;
  bool opening_swap_drop() const;
  Bitboard opening_swap_drop_targets(Color c, PieceType pt) const;
  bool is_opening_self_removal_move(Move m) const;
  bool piece_drops() const;
  Color drop_hand_color(Color c, PieceType pt) const;
  bool drop_loop() const;
  bool captures_to_hand() const;
  PieceSet capture_to_hand_types() const;
  PieceSet self_destruct_types() const;
  PieceSet clone_move_types() const;
  bool can_clone(Piece p) const;
  Bitboard clone_targets_from(Color c, Square from) const;
  Bitboard pull_sources_from(Color c, Square from) const;
  Bitboard pull_targets_from(Color c, Square from, Square pullFrom) const;
  Bitboard adjacent_swap_targets_from(Color c, Square from) const;
  PieceType first_move_piece_type(PieceType pt) const;
  bool first_move_lose_on_check() const;
  bool first_rank_pawn_drops() const;
  bool can_drop(Color c, PieceType pt) const;
  bool has_exchange() const;
  PieceSet rescueFor(PieceType pt) const;
  CapturingRule capture_type() const;
  PieceSet jump_capture_types() const;
  bool forced_jump_continuation() const;
  bool forced_jump_same_direction() const;
  EnclosingRule enclosing_drop() const;
  Bitboard drop_region(Color c) const;
  Bitboard drop_region(Color c, PieceType pt) const;
  bool sittuyin_rook_drop() const;
  bool drop_opposite_colored_bishop() const;
  bool drop_promoted() const;
  PieceSet drop_piece_types(PieceType pt) const;
  PieceSet symmetric_drop_types() const;
  PieceSet capture_drop_types() const;
  PieceType drop_no_doubled() const;
  PieceType drop_no_doubled(Color c) const;
  PieceSet promotion_pawn_types(Color c) const;
  PieceSet pawn_like_types(Color c) const;
  PieceSet en_passant_types(Color c) const;
  bool immobility_illegal() const;
  bool potions_enabled() const;
  PieceType potion_piece(Variant::PotionType type) const;
  bool can_cast_potion(Color c, Variant::PotionType type) const;
  Bitboard potion_zone(Color c, Variant::PotionType type) const;
  int potion_cooldown(Color c, Variant::PotionType type) const;
  Bitboard freeze_squares() const;
  Bitboard freeze_squares(Color c) const;
  Bitboard jump_squares(Color c) const;
  Bitboard freeze_zone_from_square(Square s) const;
  bool gating() const;
  bool gating_from_hand() const;
  PieceType gating_piece_after(Color c, PieceType pt) const;
  PieceType forced_gating_type(Color c, PieceType pt) const;
  bool walling() const;
  bool walling(Color c) const;
  WallingRule walling_rule() const;
  bool wall_or_move() const;
  Bitboard walling_region(Color c) const;
  bool seirawan_gating() const;
  bool commit_gates() const;
  bool cambodian_moves() const;
  Bitboard diagonal_lines() const;
  bool pass(Color c) const;
  bool has_setup_drop(Color c) const;
  bool pass_until_setup() const;
  bool pass_on_stalemate(Color c) const;
  bool multimove_pass(int ply) const;
  bool has_forced_jump_followup() const;
  Square forced_jump_square() const;
  Bitboard promoted_soldiers(Color c) const;
  bool makpong() const;
  EnclosingRule flip_enclosed_pieces() const;
  // winning conditions
  int n_move_rule() const;
  int n_move_rule_immediate() const;
  int n_move_hard_limit_rule() const;
  Value n_move_hard_limit_rule_value() const;
  int n_fold_rule() const;
  int n_fold_rule_immediate() const;
  Value stalemate_value(int ply = 0) const;
  Value checkmate_value(int ply = 0) const;
  Value extinction_value(int ply = 0) const;
  bool extinction_claim() const;
  PieceSet extinction_piece_types() const;
  PieceSet extinction_piece_types(Color c) const;
  PieceSet extinction_must_appear() const;
  bool extinction_all_piece_types(Color c) const;
  bool extinction_single_piece() const;
  int extinction_piece_count() const;
  int extinction_piece_count(Color c) const;
  int extinction_opponent_piece_count() const;
  int extinction_opponent_piece_count(Color c) const;
  PieceSet pseudo_royal_types() const;
  int pseudo_royal_count() const;
  Value pseudo_royal_value(int ply = 0) const;
  PieceSet anti_royal_types() const;
  int anti_royal_count() const;
  bool anti_royal_self_capture_only() const;
  bool anti_royal_king_mutually_immune() const;
  bool extinction_pseudo_royal() const;
  PieceType flag_piece(Color c) const;
  Bitboard flag_region(Color c) const;
  bool flag_move() const;
  bool flag_reached(Color c) const;
  bool check_counting() const;
  int connect_n() const;
  PieceSet connect_piece_types() const;
  bool connect_goal_by_type() const;
  const std::vector<PieceType>& connect_piece_goal_types(Color c) const;
  bool connect_horizontal() const;
  bool connect_vertical() const;
  bool connect_diagonal() const;
  bool weak_diagonal_connect() const;
  const std::vector<Direction>& getConnectDirections() const;
  const std::vector<std::vector<Square>>& getConnectLines() const;
  int connect_nxn() const;
  int collinear_n() const;
  int connect_group() const;
  Value connect_value() const;
  bool points_counting() const;
  bool pay_points_to_drop() const;
  PointsRule points_rule_captures() const;
  int points_goal() const;
  int points_count(Color c) const;
  int points_score(Color c) const;
  int points_score_clamped(Color c) const;
  Value points_goal_value() const;
  Value points_goal_simul_value_by_most_points() const;
  Value points_goal_simul_value_by_mover() const;

  CheckCount checks_remaining(Color c) const;
  MaterialCounting material_counting() const;
  CountingRule counting_rule() const;

  // Variant-specific properties
  int count_in_hand(PieceType pt) const;
  int count_in_hand(Color c, PieceType pt) const;
  int count_with_hand(Color c, PieceType pt) const;
  int count_in_prison(Color c, PieceType pt) const;
  bool prison_pawn_promotion() const;
  bool bikjang() const;
  bool virtual_drops() const;
  bool allow_virtual_drop(Color c, PieceType pt) const;

  // Position representation
  Bitboard pieces(PieceType pt = ALL_PIECES) const;
  Bitboard pieces(PieceType pt1, PieceType pt2) const;
  Bitboard pieces(Color c) const;
  Bitboard pieces(Color c, PieceType pt) const;
  Bitboard pieces(Color c, PieceType pt1, PieceType pt2) const;
  Bitboard pieces(Color c, PieceType pt1, PieceType pt2, PieceType pt3) const;
  Bitboard major_pieces(Color c) const;
  Bitboard non_sliding_riders() const;
  Piece piece_on(Square s) const;
  Piece unpromoted_piece_on(Square s) const;
  Bitboard ep_squares() const;
  Square castling_king_square(Color c) const;
  Bitboard gates(Color c) const;
  Square gate_square(Move m) const;
  bool empty(Square s) const;
  int count(Color c, PieceType pt) const;
  template<PieceType Pt> int count(Color c) const;
  template<PieceType Pt> int count() const;
  template<PieceType Pt> Square square(Color c) const;
  Square square(Color c, PieceType pt) const;
  bool is_on_semiopen_file(Color c, Square s) const;

  // Castling
  CastlingRights castling_rights(Color c) const;
  bool can_castle(CastlingRights cr) const;
  bool castling_impeded(CastlingRights cr) const;
  Square castling_rook_square(CastlingRights cr) const;

  // Checking
  Bitboard checkers() const;
  Bitboard evasion_checkers() const;
  Bitboard blockers_for_king(Color c) const;
  Bitboard check_squares(PieceType pt) const;
  Bitboard pinners(Color c) const;
  Bitboard checked_pseudo_royals(Color c) const;
  Bitboard checked_anti_royals(Color c) const;

  // Attacks to/from a given square
  Bitboard attackers_to(Square s) const;
  Bitboard attackers_to(Square s, Color c) const;
  Bitboard attackers_to(Square s, Bitboard occupied) const;
  Bitboard attackers_to(Square s, Bitboard occupied, Color c) const;
  Bitboard attackers_to(Square s, Bitboard occupied, Color c, Bitboard janggiCannons) const;
  Bitboard attackers_to_king(Square s, Color c) const;
  Bitboard attackers_to_king(Square s, Bitboard occupied, Color c) const;
  Bitboard attackers_to_king(Square s, Bitboard occupied, Color c, Bitboard janggiCannons) const;
  Bitboard attacks_from(Color c, PieceType pt, Square s) const;
  Bitboard attacks_from(Color c, PieceType pt, Square s, Bitboard occupancy) const;
  Bitboard moves_from(Color c, PieceType pt, Square s) const;
  Bitboard push_targets_from(Color c, PieceType pt, Square s) const;
  Bitboard slider_blockers(Bitboard sliders, Square s, Bitboard& pinners, Color c) const;

  // Properties of moves
  bool legal(Move m) const;
  bool pseudo_legal(const Move m) const;
  bool virtual_drop(Move m) const;
  bool paired_drop(Move m) const;
  bool push_move(Move m) const;
  bool push_captures(Move m) const;
  bool push_ejects(Move m) const;
  Square push_capture_square(Move m) const;
  bool capture(Move m) const;
  bool capture_or_promotion(Move m) const;
  bool is_jump_capture(Move m) const;
  Square capture_square(Square to) const;
  Square capture_square(Move m) const;
  Square secondary_drop_square(Move m) const;
  Square mirrored_pair_drop_square(Square s) const;
  Square jump_capture_square(Square from, Square to) const;
  bool gives_check(Move m) const;
  Piece moved_piece(Move m) const;
  bool is_clone_move(Move m) const;
  bool is_pull_move(Move m) const;
  bool is_swap_move(Move m) const;
  bool is_first_move_special(Move m) const;
  Piece captured_piece() const;
  Piece captured_piece(Move m) const;
  const std::string piece_to_partner() const;
  PieceType committed_piece_type(Move m, bool castlingRook) const;

  // Piece specific
  bool pawn_passed(Color c, Square s) const;
  bool opposite_bishops() const;
  bool is_promoted(Square s) const;
  int  pawns_on_same_color_squares(Color c, Square s) const;

  // Doing and undoing moves
  void do_move(Move m, StateInfo& newSt);
  void do_move(Move m, StateInfo& newSt, bool givesCheck);
  void undo_move(Move m);
  void do_null_move(StateInfo& newSt);
  void undo_null_move();

  // Static Exchange Evaluation
  Value blast_see(Move m) const;
  bool see_ge(Move m, Value threshold = VALUE_ZERO) const;

  // Accessing hash keys
  Key key() const;
  Key key_after(Move m) const;
  Key material_key(EndgameEval e = EG_EVAL_CHESS) const;
  Key pawn_key() const;

  // Other properties of the position
  Color side_to_move() const;
  int game_ply() const;
  bool is_chess960() const;
  Thread* this_thread() const;
  bool is_immediate_game_end() const;
  bool is_immediate_game_end(Value& result, int ply = 0) const;
  bool is_optional_game_end() const;
  bool is_optional_game_end(Value& result, int ply = 0, int countStarted = 0) const;
  bool is_game_end(Value& result, int ply = 0) const;
  Value material_counting_result() const;
  int connect_line_count(Color c) const;
  bool is_draw(int ply) const;
  bool has_game_cycle(int ply) const;
  bool has_repeated() const;
  bool see_pruning_unreliable() const;
  Bitboard chased() const;
  int count_limit(Color sideToCount) const;
  int board_honor_counting_ply(int countStarted) const;
  bool board_honor_counting_shorter(int countStarted) const;
  int counting_limit(int countStarted) const;
  int counting_ply(int countStarted) const;
  int rule50_count() const;
  Score psq_score() const;
  Value non_pawn_material(Color c) const;
  Value non_pawn_material() const;
  Bitboard not_moved_pieces(Color c) const;
  Bitboard wall_squares() const;
  Bitboard fog_area() const;

  // Position consistency check, for debugging
  bool pos_is_ok() const;
  bool material_key_is_ok() const;
  void refresh_state_derived(StateInfo* si) const;
  void flip();

  // Used by NNUE
  StateInfo* state() const;

  void put_piece(Piece pc, Square s, bool isPromoted = false, Piece unpromotedPc = NO_PIECE, bool markNotMoved = false);
  void remove_piece(Square s);

private:
  // Initialization helpers (used while setting up a position)
  void set_castling_right(Color c, Square rfrom);
  void set_state(StateInfo* si) const;
  void recompute_state_hashes_and_material(StateInfo* si) const;
  Key compute_material_key() const;
  Bitboard compute_checkers_bb(Color side) const;
  Bitboard compute_evasion_checkers_bb(Color side) const;
  void set_check_info(StateInfo* si) const;
  bool compute_forced_jump_followup(Square s, int step = 0) const;
  bool is_initial_pawn(Piece pc, Square s) const;
  Key layout_key() const;
  bool violates_same_player_board_repetition(Move m) const;
  Key reserve_key() const;
  bool n_fold_game_end(Value& result, int ply, int target) const;
  Bitboard passive_blast_checkers(Color victim, Bitboard occupied) const;

  // Other helpers
  void move_piece(Square from, Square to);
  template<bool Do>
  void do_castling(Color us, Square from, Square& to, Square& rfrom, Square& rto);
  static Bitboard dynamic_slider_bb(const std::map<Direction,int>& directions,
                                    Square sq, Bitboard blockers,
                                    Bitboard occupiedAll, Color c);
  static Bitboard max_slider_bb(const std::map<Direction,int>& directions,
                                Square sq, Bitboard occupied,
                                Bitboard ownPieces, Color c,
                                bool captureMode,
                                bool includeOwnBlockedAttacks);
  static Bitboard contra_hopper_bb(const std::map<Direction,int>& directions,
                                   Square sq, Bitboard occupied,
                                   Bitboard ownPieces, Color c,
                                   bool quietMode,
                                   bool includeOwnBlockedAttacks);
  static std::pair<int, int> decode_direction(Direction d);
  static Bitboard wrapped_step_targets(const std::map<Direction, int>& directions,
                                       Square sq, Bitboard occupied,
                                       File maxFile, Rank maxRank,
                                       bool wrapFile, bool wrapRank,
                                       bool requireEmpty);
  static Bitboard wrapped_tuple_targets(const std::vector<std::pair<int, int>>& steps,
                                        Color c, Square sq, Bitboard occupied,
                                        File maxFile, Rank maxRank,
                                        bool wrapFile, bool wrapRank,
                                        bool requireEmpty);
  static Bitboard wrapped_tuple_rider_targets(const std::vector<PieceInfo::TupleRay>& rays,
                                              Color c, Square sq, Bitboard occupied,
                                              File maxFile, Rank maxRank,
                                              bool wrapFile, bool wrapRank,
                                              bool quietMode);
  static Bitboard wrapped_slider_targets(const std::map<Direction, int>& directions,
                                         Square sq, Bitboard occupied,
                                         File maxFile, Rank maxRank,
                                         bool wrapFile, bool wrapRank,
                                         bool quietMode);
  static Bitboard wrapped_hopper_targets(const std::map<Direction, int>& directions,
                                         Square sq, Bitboard occupied,
                                         File maxFile, Rank maxRank,
                                         bool wrapFile, bool wrapRank,
                                         bool quietMode);
  static Bitboard wrapped_contra_hopper_targets(const std::map<Direction, int>& directions,
                                                Color c, Square sq, Bitboard occupied, Bitboard ownPieces,
                                                File maxFile, Rank maxRank,
                                                bool wrapFile, bool wrapRank,
                                                bool quietMode,
                                                bool includeOwnBlockedAttacks);
  static Bitboard wrapped_bent_rider_targets(bool griffon, Square sq, Bitboard occupied,
                                             File maxFile, Rank maxRank,
                                             bool wrapFile, bool wrapRank,
                                             bool quietMode);
  static Bitboard wrapped_leap_rider_targets(const std::map<Direction, int>& directions,
                                             Color c, Square sq, Bitboard occupied,
                                             File maxFile, Rank maxRank,
                                             bool wrapFile, bool wrapRank,
                                             bool quietMode);
  static Bitboard wrapped_rose_targets(Square sq, Bitboard occupied,
                                       File maxFile, Rank maxRank,
                                       bool wrapFile, bool wrapRank,
                                       bool quietMode);
  static Bitboard special_rider_bb(const PieceInfo* pi, MoveModality modality,
                                   Square sq, Bitboard occupied,
                                   Bitboard occupiedAll, Bitboard ownPieces,
                                   Color c, bool captureMode,
                                   bool includeOwnBlockedAttacks = false);

  // Data members
  Piece board[SQUARE_NB];
  Piece unpromotedBoard[SQUARE_NB];
  Bitboard byTypeBB[PIECE_TYPE_NB];
  Bitboard byColorBB[COLOR_NB];
  int pieceCount[PIECE_NB];
  int castlingRightsMask[SQUARE_NB];
  Square castlingRookSquare[CASTLING_RIGHT_NB];
  Bitboard castlingPath[CASTLING_RIGHT_NB];
  Thread* thisThread;
  StateInfo* st;
  int gamePly;
  Color sideToMove;
  Score psq;

  // variant-specific
  const Variant* var;
  bool tsumeMode;
  bool chess960;
  int pieceCountInHand[COLOR_NB][PIECE_TYPE_NB];
  int pieceCountInPrison[COLOR_NB][PIECE_TYPE_NB];
  Bitboard pawnCannotCheckZone[COLOR_NB];
  PieceType committedGates[COLOR_NB][FILE_NB];
  int priorityDropCountInHand[COLOR_NB];
  int virtualPieces;
  Bitboard promotedPieces;
  void add_to_hand(Piece pc);
  void remove_from_hand(Piece pc);
  int add_to_prison(Piece pc);
  int remove_from_prison(Piece pc);
  void updatePawnCheckZone();
  void drop_piece(Piece pc_hand, Piece pc_drop, Square s, PieceType exchange);
  void undrop_piece(Piece pc_hand, Square s, PieceType exchange);
  void commit_piece(Piece pc, File fl);
  PieceType uncommit_piece(Color cl, File fl);
  PieceType committed_piece_type(Color cl, File fl) const;
  bool has_committed_piece(Color cl, File fl) const;
  PieceType drop_committed_piece(Color cl, File fl);
  Bitboard find_drop_region(Direction dir, Square s, Bitboard occupied) const;
  void swap_piece(Square from, Square to);
};

extern std::ostream& operator<<(std::ostream& os, const Position& pos);

inline const Variant* Position::variant() const {
  assert(var != nullptr);
  return var;
}

inline Rank Position::max_rank() const {
  assert(var != nullptr);
  return var->maxRank;
}

inline File Position::max_file() const {
  assert(var != nullptr);
  return var->maxFile;
}

inline int Position::ranks() const {
  assert(var != nullptr);
  return var->maxRank + 1;
}

inline int Position::files() const {
  assert(var != nullptr);
  return var->maxFile + 1;
}

inline bool Position::two_boards() const {
  assert(var != nullptr);
  return var->twoBoards;
}

inline Bitboard Position::board_bb() const {
  assert(var != nullptr);
  return board_size_bb(var->maxFile, var->maxRank) & ~st->wallSquares;
}

inline Bitboard Position::dead_squares() const {
  return st->deadSquares;
}

inline Bitboard Position::board_bb(Color c, PieceType pt) const {
  assert(var != nullptr);
  return var->mobilityRegion[c][pt] ? var->mobilityRegion[c][pt] & board_bb() : board_bb();
}

inline PieceSet Position::piece_types() const {
  assert(var != nullptr);
  return var->pieceTypes;
}

inline const std::string& Position::piece_to_char() const {
  assert(var != nullptr);
  return var->pieceToChar;
}

inline const std::string& Position::piece_to_char_synonyms() const {
  assert(var != nullptr);
  return var->pieceToCharSynonyms;
}

inline const std::string& Position::piece_symbol(Piece pc) const {
  assert(var != nullptr);
  return var->piece_symbol(pc);
}

inline const std::string& Position::piece_symbol_synonym(Piece pc) const {
  assert(var != nullptr);
  return var->piece_symbol_synonym(pc);
}

inline Piece Position::piece_from_symbol(const std::string& token) const {
  assert(var != nullptr);
  return var->piece_from_symbol(token);
}

inline PieceType Position::piece_type_from_symbol(const std::string& token) const {
  assert(var != nullptr);
  return var->piece_type_from_symbol(token);
}

inline Bitboard Position::promotion_zone(Color c) const {
  assert(var != nullptr);
  return var->promotionRegion[c];
}

inline Bitboard Position::promotion_zone(Color c, PieceType pt) const {
    assert(var != nullptr);
    assert(pt != NO_PIECE_TYPE);
    Bitboard b = var->promotionRegion[c];

    // Piece specific promotion region
    // Only filter moves based on var->promotionRegion[] which is a restriction that applies to all pieces
    // Set whitePromotionRegion/blackPromotionRegion to AllSquares to remove the restriction
    if (var->pieceSpecificPromotionRegion)
    {
        if (c == WHITE)
        {
            b &= var->whitePiecePromotionRegion.boardOfPiece(toupper(piece_to_char()[(c << PIECE_TYPE_BITS) | pt]));
        }
        else if (c == BLACK)
        {
            b &= var->blackPiecePromotionRegion.boardOfPiece(toupper(piece_to_char()[(c << PIECE_TYPE_BITS) | pt]));
        }
    }
    return b;
}

inline Bitboard Position::promotion_zone(Piece p) const {
    assert(var != nullptr);
    assert(p != NO_PIECE);
    return promotion_zone(color_of(p), type_of(p));
}

inline Bitboard Position::mandatory_promotion_zone(Color c) const {
  assert(var != nullptr);
  return var->mandatoryPromotionRegion[c];
}

inline Bitboard Position::mandatory_promotion_zone(Color c, PieceType pt) const {
  assert(var != nullptr);
  return mandatory_promotion_zone(c) & promotion_zone(c, pt);
}

inline Bitboard Position::mandatory_promotion_zone(Piece p) const {
  assert(var != nullptr);
  assert(p != NO_PIECE);
  return mandatory_promotion_zone(color_of(p), type_of(p));
}

inline Square Position::promotion_square(Color c, Square s) const {
  assert(var != nullptr);
  // Return the nearest promotion-zone square for the piece currently on `s`,
  // searching along color `c`'s forward file. Callers should pass a square
  // occupied by a piece of color `c`; empty or mismatched squares return SQ_NONE.
  Piece p = piece_on(s);
  Bitboard b = ((p == NO_PIECE) ? Bitboard(0) : promotion_zone(p)) & forward_file_bb(c, s) & board_bb();
  return !b ? SQ_NONE : c == WHITE ? lsb(b) : msb(b);
}

inline PieceType Position::main_promotion_pawn_type(Color c) const {
  assert(var != nullptr);
  return var->mainPromotionPawnType[c];
}

inline PieceSet Position::promotion_piece_types(Color c) const {
  assert(var != nullptr);
  if (var->promotionPieceTypesByFileEnabled[c])
  {
      PieceSet unionSet = NO_PIECE_SET;
      for (int f = FILE_A; f <= int(var->maxFile); ++f)
          unionSet |= var->promotionPieceTypesByFile[c][f];
      return unionSet;
  }
  return var->promotionPieceTypes[c];
}

inline PieceSet Position::promotion_piece_types(Color c, Square s) const {
  assert(var != nullptr);
  if (var->promotionPieceTypesByFileEnabled[c] && s != SQ_NONE)
  {
      File f = file_of(s);
      if (f >= FILE_A && f <= var->maxFile)
          return var->promotionPieceTypesByFile[c][f];
  }
  return promotion_piece_types(c);
}

inline bool Position::sittuyin_promotion() const {
  assert(var != nullptr);
  return var->sittuyinPromotion;
}

inline int Position::promotion_limit(PieceType pt) const {
  assert(var != nullptr);
  return var->promotionLimit[pt];
}

inline bool Position::promotion_allowed(Color c, PieceType pt) const {
  if (promotion_limit(pt) && promotion_limit(pt) <= count(c, pt))
      return false;
  if (var->promotionSteal && count(~c, pt) == 0)
      return false;
  if ((var->promotionRequireInHand || var->promotionConsumeInHand) && count_in_hand(c, pt) <= 0)
      return false;
  return true;
}

inline bool Position::promotion_allowed(Color c, PieceType pt, Square s) const {
  return bool(promotion_piece_types(c, s) & pt) && promotion_allowed(c, pt);
}

inline PieceType Position::promoted_piece_type(PieceType pt) const {
  assert(var != nullptr);
  return var->promotedPieceType[pt];
}

inline bool Position::piece_promotion_on_capture() const {
  assert(var != nullptr);
  return var->piecePromotionOnCapture;
}

inline bool Position::mandatory_pawn_promotion() const {
  assert(var != nullptr);
  if (var->mandatoryPawnPromotionByColorSet[WHITE] || var->mandatoryPawnPromotionByColorSet[BLACK])
      return var->mandatoryPawnPromotionByColor[side_to_move()];
  return var->mandatoryPawnPromotion;
}

inline bool Position::mandatory_piece_promotion() const {
  assert(var != nullptr);
  if (var->mandatoryPiecePromotionByColorSet[WHITE] || var->mandatoryPiecePromotionByColorSet[BLACK])
      return var->mandatoryPiecePromotionByColor[side_to_move()];
  return var->mandatoryPiecePromotion;
}

inline bool Position::piece_demotion() const {
  assert(var != nullptr);
  return var->pieceDemotion;
}

inline bool Position::blast_on_capture() const {
  assert(var != nullptr);
  return var->blastOnCapture;
}

inline bool Position::blast_on_capture(Move m) const {
  return blast_on_capture(moved_piece(m), captured_piece(m));
}

inline bool Position::blast_on_capture(Piece mover, Piece captured) const {
  assert(var != nullptr);
  if (var->blastOnCapture)
      return true;
  if (!var->blastOnSameTypeCapture || mover == NO_PIECE || captured == NO_PIECE)
      return false;
  return type_of(captured) == type_of(mover);
}

inline bool Position::blast_on_move() const {
  assert(var != nullptr);
  return var->blastOnMove;
}

inline bool Position::blast_on_self_destruct() const {
  assert(var != nullptr);
  return var->blastOnSelfDestruct;
}

inline bool Position::blast_promotion() const {
  assert(var != nullptr);
  return var->blastPromotion;
}

inline bool Position::blast_diagonals() const {
  assert(var != nullptr);
  return var->blastDiagonals;
}

inline bool Position::blast_orthogonals() const {
  assert(var != nullptr);
  return var->blastOrthogonals;
}

inline bool Position::blast_center() const {
  assert(var != nullptr);
  return var->blastCenter;
}

inline bool Position::zero_range_blast_on_capture(Move m) const {
  return zero_range_blast_on_capture(moved_piece(m), captured_piece(m));
}

inline bool Position::zero_range_blast_on_capture(Piece mover, Piece captured) const {
  assert(var != nullptr);
  return blast_on_capture(mover, captured) && blast_center() && !blast_orthogonals() && !blast_diagonals();
}

inline PieceSet Position::blast_immune_types() const {
  assert(var != nullptr);
  return var->blastImmuneTypes;
}

inline PieceSet Position::death_on_capture_types() const {
  assert(var != nullptr);
  return var->deathOnCaptureTypes;
}

inline Bitboard Position::blast_immune_bb() const {
    Bitboard blastImmune = 0;
    for (PieceSet ps = blast_immune_types(); ps;) {
        PieceType pt = pop_lsb(ps);
        blastImmune |= pieces(pt);
    }
    return blastImmune;
}

inline Bitboard Position::blast_pattern(Square to) const {
    Bitboard blastPattern = 0;
    if (blast_orthogonals())
        blastPattern |= attacks_bb<WAZIR>(to);
    if (blast_diagonals())
        blastPattern |= attacks_bb<KING>(to) & ~attacks_bb<WAZIR>(to);
    return blastPattern;
}

inline Bitboard Position::blast_squares(Square to) const {
    Bitboard blastImmune = blast_immune_bb();
    Bitboard blastPattern = blast_pattern(to);
    Bitboard relevantPieces = (pieces(WHITE) | pieces(BLACK)) ^ pieces(PAWN);
    Bitboard blastArea = (blastPattern & relevantPieces) | (blast_center() ? square_bb(to) : Bitboard(0));

    return blastArea & (pieces() ^ blastImmune);
}

inline int Position::remove_connect_n() const {
  assert(var != nullptr);
  return var->removeConnectN;
}

inline bool Position::remove_connect_n_by_type() const {
  assert(var != nullptr);
  return var->removeConnectNByType;
}

inline PieceSet Position::mutually_immune_types() const {
  assert(var != nullptr);
  return var->mutuallyImmuneTypes;
}

inline bool Position::surround_capture_opposite() const {
  assert(var != nullptr);
  return var->surroundCaptureOpposite;
}

inline bool Position::surround_capture_intervene() const {
  assert(var != nullptr);
  return var->surroundCaptureIntervene;
}

inline bool Position::surround_capture_edge() const {
  assert(var != nullptr);
  return var->surroundCaptureEdge;
}

inline Bitboard Position::surround_capture_max_region() const {
  assert(var != nullptr);
  return var->surroundCaptureMaxRegion;
}

inline Bitboard Position::surround_capture_hostile_region() const {
  assert(var != nullptr);
  return var->surroundCaptureHostileRegion;
}

inline EndgameEval Position::endgame_eval() const {
  assert(var != nullptr);
  return !count_in_hand(ALL_PIECES) && (var->endgameEval != EG_EVAL_CHESS || count<KING>() == 2) ? var->endgameEval : NO_EG_EVAL;
}

inline Bitboard Position::double_step_region(Color c) const {
  assert(var != nullptr);
  return var->doubleStepRegion[c];
}

inline Bitboard Position::double_step_region(Color c, PieceType pt) const {
    assert(var != nullptr);
    assert(pt != NO_PIECE_TYPE);
    Bitboard b = var->doubleStepRegion[c];

    // Piece-specific multi-step regions override the global region for that
    // piece type when an explicit per-piece region is configured.
    if (var->pieceSpecificDoubleStepRegion)
    {
        if (c == WHITE)
        {
            Bitboard pieceSpecific = var->whitePieceDoubleStepRegion.boardOfPiece(toupper(piece_to_char()[(c << PIECE_TYPE_BITS) | pt]));
            if (pieceSpecific)
                b = pieceSpecific;
        }
        else if (c == BLACK)
        {
            Bitboard pieceSpecific = var->blackPieceDoubleStepRegion.boardOfPiece(toupper(piece_to_char()[(c << PIECE_TYPE_BITS) | pt]));
            if (pieceSpecific)
                b = pieceSpecific;
        }
    }

    return b;
}

inline Bitboard Position::double_step_region(Piece p) const {
    assert(var != nullptr);
    assert(p != NO_PIECE);
    return double_step_region(color_of(p), type_of(p));
}

inline Bitboard Position::triple_step_region(Color c) const {
  assert(var != nullptr);
  return var->tripleStepRegion[c];
}

inline Bitboard Position::triple_step_region(Color c, PieceType pt) const {
    assert(var != nullptr);
    assert(pt != NO_PIECE_TYPE);
    Bitboard b = var->tripleStepRegion[c];

    // Piece-specific multi-step regions override the global region for that
    // piece type when an explicit per-piece region is configured.
    if (var->pieceSpecificTripleStepRegion)
    {
        if (c == WHITE)
        {
            Bitboard pieceSpecific = var->whitePieceTripleStepRegion.boardOfPiece(toupper(piece_to_char()[(c << PIECE_TYPE_BITS) | pt]));
            if (pieceSpecific)
                b = pieceSpecific;
        }
        else if (c == BLACK)
        {
            Bitboard pieceSpecific = var->blackPieceTripleStepRegion.boardOfPiece(toupper(piece_to_char()[(c << PIECE_TYPE_BITS) | pt]));
            if (pieceSpecific)
                b = pieceSpecific;
        }
    }

    return b;
}

inline Bitboard Position::triple_step_region(Piece p) const {
    assert(var != nullptr);
    assert(p != NO_PIECE);
    return triple_step_region(color_of(p), type_of(p));
}

inline bool Position::castling_enabled() const {
  assert(var != nullptr);
  return var->castling;
}

inline bool Position::castling_dropped_piece() const {
  assert(var != nullptr);
  return var->castlingDroppedPiece;
}

inline File Position::castling_kingside_file() const {
  assert(var != nullptr);
  return var->castlingKingsideFile;
}

inline File Position::castling_queenside_file() const {
  assert(var != nullptr);
  return var->castlingQueensideFile;
}

inline Rank Position::castling_rank(Color c) const {
  assert(var != nullptr);
  return relative_rank(c, var->castlingRank, max_rank());
}

inline File Position::castling_king_file() const {
  assert(var != nullptr);
  return var->castlingKingFile;
}

inline PieceType Position::castling_king_piece(Color c) const {
  assert(var != nullptr);
  return var->castlingKingPiece[c];
}

inline PieceSet Position::castling_rook_pieces(Color c) const {
  assert(var != nullptr);
  return var->castlingRookPieces[c];
}

inline PieceType Position::king_type() const {
  assert(var != nullptr);
  return var->kingType;
}

inline PieceType Position::nnue_king() const {
  assert(var != nullptr);
  return var->nnueKing;
}

inline Square Position::nnue_king_square(Color c) const {
  return nnue_king() ? square(c, nnue_king()) : SQ_NONE;
}

inline bool Position::nnue_use_pockets() const {
  assert(var != nullptr);
  return var->nnueUsePockets;
}

inline bool Position::nnue_applicable() const {
  // Do not use NNUE during setup phases (placement, sittuyin)
  return (!count_in_hand(ALL_PIECES) || nnue_use_pockets() || !must_drop())
         && !virtualPieces
         && (!nnue_king() || (count(WHITE, nnue_king()) == 1 && count(BLACK, nnue_king()) == 1));
}

inline int Position::nnue_piece_square_index(Color perspective, Piece pc) const {
  assert(var != nullptr);
  return var->pieceSquareIndex[perspective][pc];
}

inline int Position::nnue_piece_hand_index(Color perspective, Piece pc) const {
  assert(var != nullptr);
  return var->pieceHandIndex[perspective][pc];
}

inline int Position::nnue_king_square_index(Square ksq) const {
  assert(var != nullptr);
  return var->kingSquareIndex[ksq];
}

inline int Position::nnue_wall_index_base() const {
  assert(var != nullptr);
  return var->nnueWallIndexBase;
}

inline int Position::nnue_points_index_base() const {
  assert(var != nullptr);
  return var->nnuePointsIndexBase;
}

inline int Position::nnue_points_score_planes() const {
  assert(var != nullptr);
  return var->nnuePointsScorePlanes;
}

inline int Position::nnue_points_check_planes() const {
  assert(var != nullptr);
  return var->nnuePointsCheckPlanes;
}

inline int Position::nnue_potion_zone_index_base() const {
  assert(var != nullptr);
  return var->nnuePotionZoneIndexBase;
}

inline int Position::nnue_potion_cooldown_index_base() const {
  assert(var != nullptr);
  return var->nnuePotionCooldownIndexBase;
}

inline bool Position::checking_permitted() const {
  assert(var != nullptr);
  return var->checking;
}

inline bool Position::allow_checks() const {
  assert(var != nullptr);
  return var->allowChecks;
}

inline bool Position::free_drops() const {
  assert(var != nullptr);
  return var->freeDrops;
}

inline bool Position::fast_attacks() const {
  assert(var != nullptr);
  return var->fastAttacks && !topology_wraps();
}

inline bool Position::fast_attacks2() const {
  assert(var != nullptr);
  return var->fastAttacks2 && !topology_wraps();
}

inline bool Position::wraps_files() const {
  assert(var != nullptr);
  return var->cylindrical || var->toroidal;
}

inline bool Position::wraps_ranks() const {
  assert(var != nullptr);
  return var->toroidal;
}

inline bool Position::topology_wraps() const {
  return wraps_files() || wraps_ranks();
}

inline bool Position::is_hex_board() const {
  assert(var != nullptr);
  return var->hexBoard;
}

inline bool Position::drop_checks() const {
  assert(var != nullptr);
  if (var->dropChecksByColorSet[WHITE] || var->dropChecksByColorSet[BLACK])
      return var->dropChecksByColor[side_to_move()];
  return var->dropChecks;
}

inline bool Position::drop_mates() const {
  assert(var != nullptr);
  if (var->dropMatesByColorSet[WHITE] || var->dropMatesByColorSet[BLACK])
      return var->dropMatesByColor[side_to_move()];
  return var->dropMates;
}

inline bool Position::shogi_pawn_drop_mate_illegal() const {
  return shogi_pawn_drop_mate_illegal(side_to_move());
}

inline bool Position::shogi_pawn_drop_mate_illegal(Color c) const {
  assert(var != nullptr);
  if (var->shogiPawnDropMateIllegalByColorSet[WHITE] || var->shogiPawnDropMateIllegalByColorSet[BLACK])
      return var->shogiPawnDropMateIllegalByColor[c];
  return var->shogiPawnDropMateIllegal;
}

inline bool Position::self_capture() const {
  assert(var != nullptr);
  Color us = side_to_move();
  if (var->selfCaptureTypesByColorSet[us])
      return var->selfCaptureTypesByColor[us] != NO_PIECE_SET;
  if (var->selfCaptureByColorSet[us])
      return var->selfCaptureByColor[us];
  if (var->selfCaptureTypes != NO_PIECE_SET)
      return var->selfCaptureTypesByColor[side_to_move()] != NO_PIECE_SET;
  return var->selfCapture;
}

inline bool Position::self_capture(PieceType pt) const {
  assert(var != nullptr);
  Color us = side_to_move();
  if (var->selfCaptureTypesByColorSet[us])
      return bool(var->selfCaptureTypesByColor[us] & piece_set(pt));
  return self_capture();
}

inline bool Position::rifle_capture() const {
  assert(var != nullptr);
  return var->rifleCapture;
}

inline bool Position::rifle_capture(Piece pc) const {
  if (pc == NO_PIECE)
      return false;

  const PieceInfo* info = pieceMap.get(type_of(pc));
  return rifle_capture() || (info && info->rifleCapture);
}

inline bool Position::rifle_capture(Move m) const {
  return rifle_capture(moved_piece(m));
}

inline int Position::pushing_strength(PieceType pt) const {
  assert(var != nullptr);
  return var->pushingStrength[pt];
}

inline bool Position::has_pushing() const {
  assert(var != nullptr);
  for (PieceSet ps = piece_types(); ps; )
      if (pushing_strength(pop_lsb(ps)) > 0)
          return true;
  return false;
}

inline int Position::pulling_strength(PieceType pt) const {
  assert(var != nullptr);
  return var->pullingStrength[pt];
}

inline bool Position::has_pulling() const {
  assert(var != nullptr);
  for (PieceSet ps = piece_types(); ps; )
      if (pulling_strength(pop_lsb(ps)) > 0)
          return true;
  return false;
}

inline PieceSet Position::adjacent_swap_move_types() const {
  assert(var != nullptr);
  return var->adjacentSwapMoveTypes;
}

inline bool Position::has_adjacent_swapping() const {
  return adjacent_swap_move_types() != NO_PIECE_SET;
}

inline bool Position::adjacent_swap_requires_empty_neighbor() const {
  assert(var != nullptr);
  return var->adjacentSwapRequiresEmptyNeighbor;
}

inline bool Position::swap_no_immediate_return() const {
  assert(var != nullptr);
  return var->swapNoImmediateReturn;
}

inline int Position::swap_forbidden_plies() const {
  assert(var != nullptr);
  return var->swapForbiddenPlies;
}

inline PushFirstColor Position::push_first_color() const {
  assert(var != nullptr);
  return var->pushFirstColor;
}

inline PushRemoval Position::pushing_removes() const {
  assert(var != nullptr);
  return var->pushingRemoves;
}

inline bool Position::push_chain_enemy_only() const {
  assert(var != nullptr);
  return var->pushChainEnemyOnly;
}

inline bool Position::push_capture_against_friendly_blocker() const {
  assert(var != nullptr);
  return var->pushCaptureAgainstFriendlyBlocker;
}

inline bool Position::push_no_immediate_return() const {
  assert(var != nullptr);
  return var->pushNoImmediateReturn;
}

inline PieceSet Position::edge_insert_types() const {
  assert(var != nullptr);
  return var->edgeInsertTypes;
}

inline bool Position::edge_insert_only() const {
  assert(var != nullptr);
  return var->edgeInsertOnly;
}

inline Bitboard Position::edge_insert_region(Color c) const {
  assert(var != nullptr);
  return var->edgeInsertRegion[c];
}

inline bool Position::edge_insert_from_top(Color c) const {
  assert(var != nullptr);
  return var->edgeInsertFromTop[c];
}

inline bool Position::edge_insert_from_bottom(Color c) const {
  assert(var != nullptr);
  return var->edgeInsertFromBottom[c];
}

inline bool Position::edge_insert_from_left(Color c) const {
  assert(var != nullptr);
  return var->edgeInsertFromLeft[c];
}

inline bool Position::edge_insert_from_right(Color c) const {
  assert(var != nullptr);
  return var->edgeInsertFromRight[c];
}

inline bool Position::capture_morph() const {
  assert(var != nullptr);
  return var->captureMorph;
}

inline bool Position::rex_exclusive_morph() const {
  assert(var != nullptr);
  return var->rexExclusiveMorph;
}

inline bool Position::must_capture() const {
  assert(var != nullptr);
  if (var->mustCaptureByColor[WHITE] || var->mustCaptureByColor[BLACK])
      return var->mustCaptureByColor[side_to_move()];
  return var->mustCapture;
}

inline bool Position::must_capture_en_passant() const {
  assert(var != nullptr);
  return var->mustCaptureEnPassant;
}

inline bool Position::has_capture() const {
  // Check for cached value
  if (st->legalCapture != NO_VALUE)
      return st->legalCapture == VALUE_TRUE;
  if (evasion_checkers())
  {
      for (const auto& mevasion : MoveList<EVASIONS>(*this))
          if (capture(mevasion) && legal(mevasion))
          {
              st->legalCapture = VALUE_TRUE;
              return true;
          }
  }
  else
  {
      for (const auto& mcap : MoveList<CAPTURES>(*this))
          if (capture(mcap) && legal(mcap))
          {
              st->legalCapture = VALUE_TRUE;
              return true;
          }
  }
  st->legalCapture = VALUE_FALSE;
  return false;
}

inline bool Position::has_en_passant_capture() const {
  if (st->legalEnPassant != NO_VALUE)
      return st->legalEnPassant == VALUE_TRUE;
  if (evasion_checkers())
  {
      for (const auto& mevasion : MoveList<EVASIONS>(*this))
          if (type_of(mevasion) == EN_PASSANT && legal(mevasion))
          {
              st->legalEnPassant = VALUE_TRUE;
              return true;
          }
  }
  else
  {
      for (const auto& mcap : MoveList<CAPTURES>(*this))
          if (type_of(mcap) == EN_PASSANT && legal(mcap))
          {
              st->legalEnPassant = VALUE_TRUE;
              return true;
          }
  }
  st->legalEnPassant = VALUE_FALSE;
  return false;
}

inline bool Position::must_drop() const {
  assert(var != nullptr);
  if (var->mustDropByColor[WHITE] || var->mustDropByColor[BLACK])
      return var->mustDropByColor[side_to_move()];
  return var->mustDrop;
}

inline PieceType Position::must_drop_type() const {
  assert(var != nullptr);
  if (var->mustDropTypeByColor[WHITE] != ALL_PIECES || var->mustDropTypeByColor[BLACK] != ALL_PIECES)
      return var->mustDropTypeByColor[side_to_move()];
  return var->mustDropType;
}

inline bool Position::opening_self_removal() const {
  assert(var != nullptr);
  return var->openingSelfRemoval;
}

inline bool Position::in_opening_self_removal_phase() const {
  return opening_self_removal() && gamePly < 2;
}

inline Bitboard Position::opening_self_removal_targets(Color c) const {
  if (!opening_self_removal() || gamePly >= 2)
      return Bitboard(0);

  Bitboard targets = pieces(c) & var->openingSelfRemovalRegion[c];
  if (gamePly == 1 && var->openingSelfRemovalAdjacentToLast)
  {
      Move lastMove = st->move;
      Square lastSq = is_ok(lastMove) ? from_sq(lastMove) : SQ_NONE;
      if (lastSq == SQ_NONE)
          return Bitboard(0);
      targets &= PseudoAttacks[WHITE][WAZIR][lastSq];
  }
  return targets;
}

inline bool Position::opening_swap_drop() const {
  assert(var != nullptr);
  return var->openingSwapDrop;
}

inline Bitboard Position::opening_swap_drop_targets(Color c, PieceType pt) const {
  if (!opening_swap_drop()
      || !st
      || !st->previous
      || st->previous->previous
      || !piece_drops()
      || !must_drop()
      || capture_type() != MOVE_OUT
      || self_capture()
      || capture_drop_types()
      || symmetric_drop_types()
      || free_drops()
      || two_boards()
      || edge_insert_types())
      return Bitboard(0);

  if (pieces(c))
      return Bitboard(0);

  Bitboard enemy = pieces(~c);
  if (popcount(enemy) != 1)
      return Bitboard(0);

  if (!(drop_piece_types(pt) & pt))
      return Bitboard(0);

  if (!var->openingSwapMirrorMainDiagonal)
      return drop_region(c, pt) & enemy;

  Square enemySq = lsb(enemy);
  Square mirrorSq = make_square(File(int(rank_of(enemySq))), Rank(int(file_of(enemySq))));
  return drop_region(c, pt) & square_bb(mirrorSq);
}

inline bool Position::is_opening_self_removal_move(Move m) const {
  return type_of(m) == SPECIAL
      && from_sq(m) == to_sq(m)
      && (opening_self_removal_targets(side_to_move()) & from_sq(m));
}

inline bool Position::piece_drops() const {
  assert(var != nullptr);
  return var->pieceDrops;
}

inline Color Position::drop_hand_color(Color c, PieceType pt) const {
  assert(var != nullptr);
  if (   var->borrowOpponentDropsWhenEmpty
      && !var->freeDrops
      && pt != ALL_PIECES
      && count_in_hand(c, ALL_PIECES) == 0
      && count_in_hand(~c, pt) > 0)
      return ~c;
  return c;
}

inline bool Position::drop_loop() const {
  assert(var != nullptr);
  return var->dropLoop;
}

inline CapturingRule Position::capture_type() const {
  assert(var != nullptr);
  return var->captureType;
}

inline PieceSet Position::jump_capture_types() const {
  assert(var != nullptr);
  return var->jumpCaptureTypes;
}

inline bool Position::forced_jump_continuation() const {
  assert(var != nullptr);
  return var->forcedJumpContinuation;
}

inline bool Position::forced_jump_same_direction() const {
  assert(var != nullptr);
  return var->forcedJumpSameDirection;
}

inline Square Position::forced_jump_square() const {
  return st->forcedJumpSquare;
}

inline bool Position::captures_to_hand() const {
  assert(var != nullptr);
  return var->captureType != MOVE_OUT;
}

inline PieceSet Position::capture_to_hand_types() const {
  assert(var != nullptr);
  return var->captureToHandTypes;
}

inline PieceSet Position::self_destruct_types() const {
  assert(var != nullptr);
  return var->selfDestructTypes;
}

inline PieceSet Position::clone_move_types() const {
  assert(var != nullptr);
  return var->cloneMoveTypes;
}

inline bool Position::can_clone(Piece p) const {
  return p != NO_PIECE && (clone_move_types() & piece_set(type_of(p)));
}

inline bool Position::first_rank_pawn_drops() const {
  assert(var != nullptr);
  return var->firstRankPawnDrops;
}

inline EnclosingRule Position::enclosing_drop() const {
  assert(var != nullptr);
  return var->enclosingDrop;
}

inline Bitboard Position::drop_region(Color c) const {
  assert(var != nullptr);
  return var->dropRegion[c];
}

inline Bitboard Position::drop_region(Color c, PieceType pt) const {
  Bitboard b = drop_region(c) & board_bb(c, pt);

  // Piece specific drop region
  // Only filter moves based on drop_region() which is a restriction that applies to all pieces
  // Set whiteDropRegion/blackDropRegion to AllSquares to remove the restriction
  if (var->pieceSpecificDropRegion)
  {
      if (c == WHITE)
      {
          b &= var->whitePieceDropRegion.boardOfPiece(toupper(piece_to_char()[(c << PIECE_TYPE_BITS) | pt]));
      }
      else if (c == BLACK)
      {
          b &= var->blackPieceDropRegion.boardOfPiece(toupper(piece_to_char()[(c << PIECE_TYPE_BITS) | pt]));
      }
  }

  // Pawns on back ranks
  if (pt == PAWN)
  {
      if (!var->promotionZonePawnDrops)
          b &= ~promotion_zone(c, pt);
      if (!first_rank_pawn_drops())
          b &= ~rank_bb(relative_rank(c, RANK_1, max_rank()));
  }
  // Doubled shogi pawns
  if (pt == drop_no_doubled(c))
      for (File f = FILE_A; f <= max_file(); ++f)
          if (popcount(file_bb(f) & pieces(c, pt)) >= var->dropNoDoubledCountByColor[c])
              b &= ~file_bb(f);
  // Sittuyin rook drops
  if (pt == ROOK && sittuyin_rook_drop())
      b &= rank_bb(relative_rank(c, RANK_1, max_rank()));

  if (enclosing_drop())
  {
      // Reversi start
      if (var->enclosingDropStart & ~pieces())
          b &= var->enclosingDropStart;
      else
      {
          // Filter out squares where the drop does not enclose at least one opponent's piece
          if (enclosing_drop() == REVERSI)
          {
              Bitboard theirs = pieces(~c);
              b &=  shift<NORTH     >(theirs) | shift<SOUTH     >(theirs)
                  | shift<NORTH_EAST>(theirs) | shift<SOUTH_WEST>(theirs)
                  | shift<EAST      >(theirs) | shift<WEST      >(theirs)
                  | shift<SOUTH_EAST>(theirs) | shift<NORTH_WEST>(theirs);
              Bitboard b2 = b;
              while (b2)
              {
                  Square s = pop_lsb(b2);
                  if (!(attacks_bb(c, QUEEN, s, board_bb() & ~pieces(~c)) & ~PseudoAttacks[c][KING][s] & pieces(c)))
                      b ^= s;
              }
          }
          else if (enclosing_drop() == SNORT)
          {
              Bitboard theirs = pieces(~c);
              b &=   ~(shift<NORTH     >(theirs) | shift<SOUTH     >(theirs)
                  | shift<EAST      >(theirs) | shift<WEST      >(theirs));
          }
          else if (enclosing_drop() == ANYSIDE)
          {
              Bitboard occupied = pieces();
              b = 0ULL;
              Bitboard candidates = (shift<WEST>(occupied) | file_bb(max_file())) & ~occupied;

              for (Rank r = RANK_1; r <= max_rank(); ++r) {
                  if (!(occupied & make_square(FILE_A, r))) {
                      b |= lsb(candidates & rank_bb(r));
                  }
              }
              candidates = (shift<SOUTH>(occupied) | rank_bb(max_rank())) & ~occupied;
              for (File f = FILE_A; f <= max_file(); ++f) {
                  if (!(occupied & make_square(f, RANK_1))) {
                      b |= lsb(candidates & file_bb(f));
                  }
              }
              candidates = (shift<NORTH>(occupied) | rank_bb(RANK_1)) & ~occupied;
              for (File f = FILE_A; f <= max_file(); ++f) {
                  if (!(occupied & make_square(f, max_rank()))) {
                      b |= lsb(candidates & file_bb(f));
                  }
              }
              candidates = (shift<EAST>(occupied) | file_bb(FILE_A)) & ~occupied;
              for (Rank r = RANK_1; r <= max_rank(); ++r) {
                  if (!(occupied & make_square(max_file(), r))) {
                      b |= lsb(candidates & rank_bb(r));
                  }
              }
          }
          else if (enclosing_drop() == TOP)
          {
              b &= shift<NORTH>(pieces()) | Rank1BB;
          }
          else
          {
              assert(enclosing_drop() == ATAXX);
              Bitboard ours = pieces(c);
              b &=  shift<NORTH     >(ours) | shift<SOUTH     >(ours)
                  | shift<NORTH_EAST>(ours) | shift<SOUTH_WEST>(ours)
                  | shift<EAST      >(ours) | shift<WEST      >(ours)
                  | shift<SOUTH_EAST>(ours) | shift<NORTH_WEST>(ours);
          }
      }
  }

  return b;
}

inline bool Position::sittuyin_rook_drop() const {
  assert(var != nullptr);
  return var->sittuyinRookDrop;
}

inline bool Position::drop_opposite_colored_bishop() const {
  assert(var != nullptr);
  return var->dropOppositeColoredBishop;
}

inline bool Position::drop_promoted() const {
  assert(var != nullptr);
  return var->dropPromoted;
}

inline PieceSet Position::drop_piece_types(PieceType pt) const {
  assert(var != nullptr);
  PieceSet forms = var->dropPieceTypes[pt];
  if (forms)
      return forms;
  forms = piece_set(pt);
  if (drop_promoted() && promoted_piece_type(pt))
      forms |= promoted_piece_type(pt);
  return forms;
}

inline PieceSet Position::symmetric_drop_types() const {
  assert(var != nullptr);
  return var->symmetricDropTypes;
}

inline PieceSet Position::capture_drop_types() const {
  assert(var != nullptr);
  return var->captureDrops;
}

inline PieceType Position::drop_no_doubled() const {
  assert(var != nullptr);
  return var->dropNoDoubledByColor[side_to_move()];
}

inline PieceType Position::drop_no_doubled(Color c) const {
  assert(var != nullptr);
  return var->dropNoDoubledByColor[c];
}

inline PieceSet Position::promotion_pawn_types(Color c) const {
  assert(var != nullptr);
  return var->promotionPawnTypes[c];
}

inline PieceSet Position::pawn_like_types(Color c) const {
  assert(var != nullptr);
  return var->promotionPawnTypes[c]
       | var->enPassantTypes[c]
       | var->nMoveRuleTypes[c]
       | piece_set(var->mainPromotionPawnType[c]);
}

inline PieceSet Position::en_passant_types(Color c) const {
  assert(var != nullptr);
  return var->enPassantTypes[c];
}

inline bool Position::immobility_illegal() const {
  assert(var != nullptr);
  return var->immobilityIllegal;
}

inline bool Position::potions_enabled() const {
  assert(var != nullptr);
  return var->potions;
}

inline PieceType Position::potion_piece(Variant::PotionType type) const {
  return var->potionPiece[type];
}

inline Bitboard Position::potion_zone(Color c, Variant::PotionType type) const {
  return st->potionZones[c][type];
}

inline int Position::potion_cooldown(Color c, Variant::PotionType type) const {
  return st->potionCooldown[c][type];
}

inline bool Position::can_cast_potion(Color c, Variant::PotionType type) const {
  if (!potions_enabled() || potion_piece(type) == NO_PIECE_TYPE)
      return false;
  if (potion_cooldown(c, type) > 0)
      return false;
  return count_in_hand(c, potion_piece(type)) > 0;
}

inline Bitboard Position::freeze_squares(Color c) const {
  if (!potions_enabled())
      return Bitboard(0);
  Bitboard mask = st->potionZones[c][Variant::POTION_FREEZE];
  if (const SpellContext* spellCtx = current_spell_context())
      mask |= spellCtx->freezeExtra;
  return mask;
}

inline Bitboard Position::freeze_squares() const {
  return freeze_squares(WHITE) | freeze_squares(BLACK);
}

inline Bitboard Position::jump_squares(Color c) const {
  if (!potions_enabled())
      return Bitboard(0);
  Bitboard mask = st->potionZones[c][Variant::POTION_JUMP];
  if (const SpellContext* spellCtx = current_spell_context(); spellCtx && c == sideToMove)
      mask |= spellCtx->jumpRemoved;
  return mask;
}

inline Bitboard Position::freeze_zone_from_square(Square s) const {
  // Implicit legacy gating moves may not carry an explicit gate square.
  if (s == SQ_NONE)
      return Bitboard(0);
  return (PseudoAttacks[WHITE][KING][s] | square_bb(s)) & board_bb();
}

inline bool Position::gating() const {
  assert(var != nullptr);
  return var->gating;
}

inline bool Position::gating_from_hand() const {
  assert(var != nullptr);
  return var->gatingFromHand;
}

inline PieceType Position::gating_piece_after(Color c, PieceType pt) const {
  assert(var != nullptr);
  return var->gatingPieceAfter[c][pt];
}

inline PieceType Position::forced_gating_type(Color c, PieceType pt) const {
  PieceType next = gating_piece_after(c, pt);
  if (next == NO_PIECE_TYPE)
      return NO_PIECE_TYPE;
  if (next == KING && count<KING>(c))
      return NO_PIECE_TYPE;
  return next;
}

inline bool Position::walling() const {
  assert(var != nullptr);
  return var->wallingRule != NO_WALLING && (var->wallingSide[WHITE] || var->wallingSide[BLACK]);
}

inline bool Position::walling(Color c) const {
  assert(var != nullptr);
  return var->wallingRule != NO_WALLING && var->wallingSide[c];
}

inline WallingRule Position::walling_rule() const {
  assert(var != nullptr);
  return var->wallingRule;
}

inline bool Position::commit_gates() const {
  assert(var != nullptr);
  return var->commitGates;
}

inline bool Position::wall_or_move() const {
  assert(var != nullptr);
  return var->wallOrMove;
}

inline Bitboard Position::walling_region(Color c) const {
  assert(var != nullptr);
  return var->wallingRegion[c];
}

inline bool Position::seirawan_gating() const {
  assert(var != nullptr);
  return var->seirawanGating;
}

inline bool Position::cambodian_moves() const {
  assert(var != nullptr);
  return var->cambodianMoves;
}

inline Bitboard Position::diagonal_lines() const {
  assert(var != nullptr);
  return var->diagonalLines;
}

inline bool Position::pass(Color c) const {
  assert(var != nullptr);
  if (st->pendingClaimPass && c == sideToMove)
      return true;
  if (forced_jump_continuation() && st->forcedJumpSquare != SQ_NONE && st->forcedJumpHasFollowup)
  {
      Piece fp = piece_on(st->forcedJumpSquare);
      if (fp != NO_PIECE && color_of(fp) != c)
          return true;
  }
  if (pass_until_setup() && must_drop()
      && !has_setup_drop(c)
      && has_setup_drop(~c))
      return true;
  return var->pass[c] || var->passOnStalemate[c]
      || ((var->multimoveOffset || var->progressiveMultimove) && multimove_pass(gamePly));
}

inline bool Position::has_setup_drop(Color c) const {
  assert(var != nullptr);

  PieceType requiredDropType =
      (var->mustDropTypeByColor[WHITE] != ALL_PIECES || var->mustDropTypeByColor[BLACK] != ALL_PIECES)
          ? var->mustDropTypeByColor[c]
          : var->mustDropType;

  auto canDropNow = [&](PieceType pt) {
      return can_drop(c, pt)
          && (!pay_points_to_drop() || st->pointsCount[c] >= var->piecePoints[pt]);
  };

  if (requiredDropType != ALL_PIECES)
      return canDropNow(requiredDropType);

  for (PieceSet ps = var->pieceTypes; ps;)
      if (canDropNow(pop_lsb(ps)))
          return true;

  return false;
}

inline bool Position::pass_until_setup() const {
  assert(var != nullptr);
  return var->passUntilSetup;
}

inline bool Position::pass_on_stalemate(Color c) const {
  assert(var != nullptr);
  return var->passOnStalemate[c];
}

// Returns whether current move is a mandatory pass to simulate multimoves
inline bool Position::multimove_pass(int ply) const {
  assert(var != nullptr);
  if (var->progressiveMultimove)
  {
      // Progressive chess turn lengths are 1,2,3,... plies per turn.
      // With mandatory pass plies this maps to odd/even offsets inside
      // segments [n^2, (n+1)^2), n starting at 0.
      int turn = int(std::sqrt(double(ply))) + 1;
      int start = (turn - 1) * (turn - 1);
      return (ply - start) & 1;
  }
  int phase = (ply - var->multimoveOffset) % var->multimoveCycle;
  return ply < var->multimoveOffset ? var->multimovePass.test(ply) : (phase + (phase >= var->multimoveCycleShift)) % 2;
}

inline Bitboard Position::promoted_soldiers(Color c) const {
  assert(var != nullptr);
  return pieces(c, SOLDIER) & zone_bb(c, var->soldierPromotionRank, max_rank());
}

inline bool Position::makpong() const {
  assert(var != nullptr);
  return var->makpongRule;
}

inline int Position::n_move_rule() const {
  assert(var != nullptr);
  return var->nMoveRule;
}

inline int Position::n_move_rule_immediate() const {
  assert(var != nullptr);
  return var->nMoveRuleImmediate;
}

inline int Position::n_move_hard_limit_rule() const {
  assert(var != nullptr);
  return var->nMoveHardLimitRule;
}

inline Value Position::n_move_hard_limit_rule_value() const {
  assert(var != nullptr);
  return var->nMoveHardLimitRuleValue;
}

inline int Position::n_fold_rule() const {
  assert(var != nullptr);
  return var->nFoldRule;
}

inline int Position::n_fold_rule_immediate() const {
  assert(var != nullptr);
  return var->nFoldRuleImmediate;
}

inline EnclosingRule Position::flip_enclosed_pieces() const {
  assert(var != nullptr);
  return var->flipEnclosedPieces;
}

inline Value Position::stalemate_value(int ply) const {
  assert(var != nullptr);
  // Check for checkmate of pseudo-royal pieces
  if (pseudo_royal_types())
  {
      Bitboard pseudoRoyals = st->pseudoRoyals & pieces(sideToMove);
      Bitboard pseudoRoyalsTheirs = st->pseudoRoyals & pieces(~sideToMove);
      while (pseudoRoyals)
      {
          Square sr = pop_lsb(pseudoRoyals);
          if (  !(blast_on_capture() && (pseudoRoyalsTheirs & blast_pattern(sr)))
              && attackers_to(sr, ~sideToMove))
              return convert_mate_value(var->checkmateValue, ply);
      }
      // Look for duple check
      if (var->dupleCheck)
      {
          Bitboard pseudoRoyalCandidates = st->pseudoRoyalCandidates & pieces(sideToMove);
          bool allCheck = bool(pseudoRoyalCandidates);
          while (allCheck && pseudoRoyalCandidates)
          {
              Square sr = pop_lsb(pseudoRoyalCandidates);
              // Touching pseudo-royal pieces are immune
              if (!(  !(blast_on_capture() && (pseudoRoyalsTheirs & blast_pattern(sr)))
                    && attackers_to(sr, ~sideToMove)))
                  allCheck = false;
          }
          if (allCheck)
              return convert_mate_value(var->checkmateValue, ply);
      }
  }
  if (anti_royal_types())
  {
      if (checked_anti_royals(sideToMove))
          return convert_mate_value(var->checkmateValue, ply);
  }
  Value result = var->stalemateValue;
  // Is piece count used to determine stalemate result?
  if (var->stalematePieceCount)
  {
      int c = count<ALL_PIECES>(sideToMove) - count<ALL_PIECES>(~sideToMove);
      result = c == 0 ? VALUE_DRAW : c < 0 ? var->stalemateValue : -var->stalemateValue;
  }
  // Apply material counting
  if (result == VALUE_DRAW && var->materialCounting)
      result = material_counting_result();
  return convert_mate_value(result, ply);
}

inline Value Position::checkmate_value(int ply) const {
  assert(var != nullptr);
  // Check for illegal mate by shogi pawn drop
  if (    shogi_pawn_drop_mate_illegal(~side_to_move())
      && !(evasion_checkers() & ~pieces(SHOGI_PAWN))
      && !st->captured.piece
      &&  st->pliesFromNull > 0
      && (st->materialKey != st->previous->materialKey))
  {
      return mate_in(ply);
  }
  // Check for shatar mate rule
  if (var->shatarMateRule)
  {
      // Mate by knight is illegal
      if (!(evasion_checkers() & ~pieces(KNIGHT)))
          return mate_in(ply);

      StateInfo* stp = st;
      while (stp->evasionCheckersBB)
      {
          // Return mate score if there is at least one shak in series of checks
          if (stp->shak)
              return convert_mate_value(var->checkmateValue, ply);

          if (stp->pliesFromNull < 2)
              break;

          stp = stp->previous->previous;
      }
      // Niol
      return VALUE_DRAW;
  }
  // Checkmate using virtual pieces
  if (two_boards() && var->checkmateValue < VALUE_ZERO)
  {
      Value virtualMaterial = VALUE_ZERO;
      for (PieceSet ps = piece_types(); ps;)
      {
          PieceType pt = pop_lsb(ps);
          virtualMaterial += std::max(-count_in_hand(~sideToMove, pt), 0) * PieceValue[MG][pt];
      }

      if (virtualMaterial > 0)
          return -VALUE_VIRTUAL_MATE + virtualMaterial / 20 + ply;
  }
  // Return mate value
  return convert_mate_value(var->checkmateValue, ply);
}

inline Value Position::extinction_value(int ply) const {
  assert(var != nullptr);
  return convert_mate_value(var->extinctionValue, ply);
}

inline bool Position::extinction_claim() const {
  assert(var != nullptr);
  return var->extinctionClaim;
}

inline PieceSet Position::extinction_piece_types() const {
  assert(var != nullptr);
  return var->extinctionPieceTypes;
}

inline PieceSet Position::extinction_piece_types(Color c) const {
  assert(var != nullptr);
  return var->extinctionPieceTypesByColor[c];
}

inline PieceSet Position::extinction_must_appear() const {
  assert(var != nullptr);
  return var->extinctionMustAppear;
}

inline bool Position::extinction_all_piece_types(Color c) const {
  assert(var != nullptr);
  return var->extinctionAllPieceTypesByColor[c];
}

inline bool Position::extinction_single_piece() const {
  assert(var != nullptr);
  return   var->extinctionValue == -VALUE_MATE
        && (var->extinctionPieceTypes & ~piece_set(ALL_PIECES));
}

inline int Position::extinction_piece_count() const {
  assert(var != nullptr);
  return var->extinctionPieceCount;
}

inline int Position::extinction_piece_count(Color c) const {
  assert(var != nullptr);
  return var->extinctionPieceCountByColor[c];
}

inline int Position::extinction_opponent_piece_count() const {
  assert(var != nullptr);
  return var->extinctionOpponentPieceCount;
}

inline int Position::extinction_opponent_piece_count(Color c) const {
  assert(var != nullptr);
  return var->extinctionOpponentPieceCountByColor[c];
}

inline PieceSet Position::pseudo_royal_types() const {
  assert(var != nullptr);
  return var->pseudoRoyalTypes;
}

inline int Position::pseudo_royal_count() const {
  assert(var != nullptr);
  return var->pseudoRoyalCount;
}

inline Value Position::pseudo_royal_value(int ply) const {
  assert(var != nullptr);
  return convert_mate_value(var->pseudoRoyalValue, ply);
}

inline PieceSet Position::anti_royal_types() const {
  assert(var != nullptr);
  return var->antiRoyalTypes;
}

inline int Position::anti_royal_count() const {
  assert(var != nullptr);
  return var->antiRoyalCount;
}

inline bool Position::anti_royal_self_capture_only() const {
  assert(var != nullptr);
  return var->antiRoyalSelfCaptureOnly;
}

inline bool Position::anti_royal_king_mutually_immune() const {
  assert(var != nullptr);
  return var->antiRoyalKingMutuallyImmune;
}

inline bool Position::extinction_pseudo_royal() const {
  return pseudo_royal_types() != NO_PIECE_SET;
}

inline PieceType Position::flag_piece(Color c) const {
  assert(var != nullptr);
  return var->flagPiece[c];
}

inline Bitboard Position::flag_region(Color c) const {
  assert(var != nullptr);
  return var->flagRegion[c];
}

inline bool Position::flag_move() const {
  assert(var != nullptr);
  return var->flagMove;
}

inline bool Position::flag_reached(Color c) const {
  assert(var != nullptr);
  bool simpleResult = 
        (flag_region(c) & pieces(c, flag_piece(c)))
        && (   popcount(flag_region(c) & pieces(c, flag_piece(c))) >= var->flagPieceCount
            || (var->flagPieceBlockedWin && !(flag_region(c) & ~pieces())));
      
  if (simpleResult&&var->flagPieceSafe)
  {
      Bitboard piecesInFlagZone = flag_region(c) & pieces(c, flag_piece(c));
      int potentialPieces = (popcount(piecesInFlagZone));
      /*
      There isn't a variant that uses it, but in the hypothetical game where the rules say I need 3
      pieces in the flag zone and they need to be safe: If I have 3 pieces there, but one is under
      threat, I don't think I can declare victory. If I have 4 there, but one is under threat, I
      think that's victory.
      */      
      while (piecesInFlagZone)
      {
          Square sr = pop_lsb(piecesInFlagZone);
          Bitboard flagAttackers = attackers_to(sr, ~c);

          if ((potentialPieces < var->flagPieceCount) || (potentialPieces >= var->flagPieceCount + 1)) break;
          while (flagAttackers)
          {
              Square currentAttack = pop_lsb(flagAttackers);
              if (legal(make_move(currentAttack, sr)))
              {
                  potentialPieces--;
                  break;
              }
          }
      }
      return potentialPieces >= var->flagPieceCount;
  }
  return simpleResult;
}

inline bool Position::check_counting() const {
  assert(var != nullptr);
  return var->checkCounting;
}

inline int Position::connect_n() const {
  assert(var != nullptr);
  return var->connectN;
}

inline PieceSet Position::connect_piece_types() const {
  assert(var != nullptr);
  return var->connectPieceTypesTrimmed;
}

inline bool Position::connect_goal_by_type() const {
  assert(var != nullptr);
  return var->connectGoalByType;
}

inline const std::vector<PieceType>& Position::connect_piece_goal_types(Color c) const {
  assert(var != nullptr);
  return var->connectPieceGoalTypes[c];
}

inline bool Position::connect_horizontal() const {
  assert(var != nullptr);
  return var->connectHorizontal;
}
inline bool Position::connect_vertical() const {
  assert(var != nullptr);
  return var->connectVertical;
}
inline bool Position::connect_diagonal() const {
  assert(var != nullptr);
  return var->connectDiagonal;
}
inline bool Position::weak_diagonal_connect() const {
  assert(var != nullptr);
  return var->weakDiagonalConnect;
}

inline const std::vector<Direction>& Position::getConnectDirections() const {
    assert(var != nullptr);
    return var->connectDirections;
}

inline const std::vector<std::vector<Square>>& Position::getConnectLines() const {
    assert(var != nullptr);
    return var->connectLines;
}

inline int Position::connect_nxn() const {
  assert(var != nullptr);
  return var->connectNxN;
}

inline int Position::collinear_n() const {
  assert(var != nullptr);
  return var->collinearN;
}

inline int Position::connect_group() const {
  assert(var != nullptr);
  return var->connectGroup;
}

inline Value Position::connect_value() const {
  assert(var != nullptr);
  return var->connectValue;
}

inline CheckCount Position::checks_remaining(Color c) const {
  return st->checksRemaining[c];
}

inline MaterialCounting Position::material_counting() const {
  assert(var != nullptr);
  return var->materialCounting;
}

inline CountingRule Position::counting_rule() const {
  assert(var != nullptr);
  return var->countingRule;
}

inline bool Position::points_counting() const {
  assert(var != nullptr);
  return var->pointsCounting;
}

inline bool Position::pay_points_to_drop() const {
  assert(var != nullptr);
  return var->payPointsToDrop;
}

inline PointsRule Position::points_rule_captures() const {
  assert(var != nullptr);
  return var->pointsRuleCaptures;
}

inline int Position::points_goal() const {
  assert(var != nullptr);
  return var->pointsGoal;
}

inline int Position::points_count(Color c) const {
  return st->pointsCount[c];
}

inline int Position::points_score(Color c) const {
  return st->pointsCount[c];
}

inline int Position::points_score_clamped(Color c) const {
  return std::max(0, std::min(points_score(c), POINTS_SCORE_MAX));
}

inline Value Position::points_goal_value() const {
  assert(var != nullptr);
  return var->pointsGoalValue;
}

inline Value Position::points_goal_simul_value_by_most_points() const {
  assert(var != nullptr);
  return var->pointsGoalSimulValueByMostPoints;
}

inline Value Position::points_goal_simul_value_by_mover() const {
  assert(var != nullptr);
  return var->pointsGoalSimulValueByMover;
}


inline bool Position::is_immediate_game_end() const {
  Value result;
  return is_immediate_game_end(result);
}

inline bool Position::is_optional_game_end() const {
  Value result;
  return is_optional_game_end(result);
}

inline bool Position::is_draw(int ply) const {
  Value result;
  return is_optional_game_end(result, ply);
}

inline bool Position::is_game_end(Value& result, int ply) const {
  return is_immediate_game_end(result, ply) || is_optional_game_end(result, ply);
}

inline Color Position::side_to_move() const {
  return sideToMove;
}

inline Piece Position::piece_on(Square s) const {
  assert(is_ok(s));
  return board[s];
}

inline bool Position::empty(Square s) const {
  return piece_on(s) == NO_PIECE;
}

inline Piece Position::unpromoted_piece_on(Square s) const {
  return unpromotedBoard[s];
}

inline Piece Position::moved_piece(Move m) const {
  if (is_drop_move(m))
      return make_piece(drop_hand_color(sideToMove, in_hand_piece_type(m)), dropped_piece_type(m));
  return piece_on(from_sq(m));
}

inline bool Position::is_clone_move(Move m) const {
  if (type_of(m) != SPECIAL || is_gating(m) || from_sq(m) == to_sq(m))
      return false;

  return can_clone(moved_piece(m));
}

inline bool Position::is_pull_move(Move m) const {
  return type_of(m) == PULL && pull_square(m) != SQ_NONE;
}

inline bool Position::is_swap_move(Move m) const {
  return type_of(m) == SWAP && from_sq(m) != to_sq(m);
}

inline PieceType Position::first_move_piece_type(PieceType pt) const {
  assert(var != nullptr);
  return var->firstMovePieceType[pt];
}

inline bool Position::first_move_lose_on_check() const {
  assert(var != nullptr);
  return var->firstMoveLoseOnCheck;
}

inline bool Position::is_first_move_special(Move m) const {
  if (type_of(m) != SPECIAL || is_gating(m) || from_sq(m) == to_sq(m))
      return false;

  Piece mover = moved_piece(m);
  if (mover == NO_PIECE)
      return false;

  PieceType pt = type_of(mover);
  PieceType extra = first_move_piece_type(pt);
  if (extra == NO_PIECE_TYPE)
      return false;

  Square from = from_sq(m);
  return (gates(side_to_move()) & from)
      && ((moves_from(side_to_move(), extra, from) | attacks_from(side_to_move(), extra, from)) & to_sq(m));
}

inline Bitboard Position::clone_targets_from(Color c, Square from) const {
  Piece mover = piece_on(from);
  if (color_of(mover) != c || !can_clone(mover))
      return 0;

  PieceType pt = type_of(mover);
  return (moves_from(c, pt, from) & ~pieces()) | (attacks_from(c, pt, from) & pieces(~c));
}

inline Bitboard Position::pull_sources_from(Color c, Square from) const {
  Piece mover = piece_on(from);
  if (mover == NO_PIECE || color_of(mover) != c)
      return 0;

  int moverStrength = pulling_strength(type_of(mover));
  if (moverStrength <= 0)
      return 0;

  Bitboard sources = PseudoAttacks[WHITE][WAZIR][from] & pieces(~c);
  Bitboard valid = 0;
  while (sources)
  {
      Square sq = pop_lsb(sources);
      Piece pulled = piece_on(sq);
      if (pulled != NO_PIECE && moverStrength > pulling_strength(type_of(pulled)))
          valid |= sq;
  }
  return valid;
}

inline Bitboard Position::pull_targets_from(Color c, Square from, Square pullFrom) const {
  if (!(pull_sources_from(c, from) & pullFrom))
      return 0;

  Piece mover = piece_on(from);
  PieceType pt = type_of(mover);
  return moves_from(c, pt, from) & ~pieces();
}

inline Bitboard Position::adjacent_swap_targets_from(Color c, Square from) const {
  Piece mover = piece_on(from);
  if (mover == NO_PIECE || color_of(mover) != c)
      return 0;
  if (!(adjacent_swap_move_types() & piece_set(type_of(mover))))
      return 0;
  if (adjacent_swap_requires_empty_neighbor() && !(PseudoAttacks[WHITE][WAZIR][from] & ~pieces()))
      return 0;
  return PseudoAttacks[WHITE][WAZIR][from] & pieces(~c);
}

inline Bitboard Position::pieces(PieceType pt) const {
  return byTypeBB[pt];
}

inline Bitboard Position::pieces(PieceType pt1, PieceType pt2) const {
  return pieces(pt1) | pieces(pt2);
}

inline Bitboard Position::pieces(Color c) const {
  return byColorBB[c];
}

inline Bitboard Position::pieces(Color c, PieceType pt) const {
  return pieces(c) & pieces(pt);
}

inline Bitboard Position::pieces(Color c, PieceType pt1, PieceType pt2) const {
  return pieces(c) & (pieces(pt1) | pieces(pt2));
}

inline Bitboard Position::pieces(Color c, PieceType pt1, PieceType pt2, PieceType pt3) const {
  return pieces(c) & (pieces(pt1) | pieces(pt2) | pieces(pt3));
}

inline Bitboard Position::major_pieces(Color c) const {
  return pieces(c) & (pieces(QUEEN) | pieces(AIWOK) | pieces(ARCHBISHOP) | pieces(CHANCELLOR) | pieces(AMAZON));
}

inline Bitboard Position::non_sliding_riders() const {
  return st->nonSlidingRiders;
}

inline int Position::count(Color c, PieceType pt) const {
  return pieceCount[make_piece(c, pt)];
}

template<PieceType Pt> inline int Position::count(Color c) const {
  return pieceCount[make_piece(c, Pt)];
}

template<PieceType Pt> inline int Position::count() const {
  return count<Pt>(WHITE) + count<Pt>(BLACK);
}

template<PieceType Pt> inline Square Position::square(Color c) const {
  assert(count<Pt>(c) == 1);
  return lsb(pieces(c, Pt));
}

inline Square Position::square(Color c, PieceType pt) const {
  assert(count(c, pt) == 1);
  return lsb(pieces(c, pt));
}

inline Bitboard Position::ep_squares() const {
  return st->epSquares;
}

inline Square Position::castling_king_square(Color c) const {
  return st->castlingKingSquare[c];
}

inline Bitboard Position::gates(Color c) const {
  assert(var != nullptr);
  return st->gatesBB[c];
}

inline Square Position::gate_square(Move m) const {
  if (seirawan_gating() && is_gating(m))
  {
      Square from = from_sq(m);
      if (type_of(m) != CASTLING)
          return from;
      Square to = to_sq(m);
      Square gate = gating_square(m);
      if (gate == from || gate == to)
          return gate;
      return from;
  }
  return gating_square(m);
}

inline bool Position::is_on_semiopen_file(Color c, Square s) const {
  return !((pieces(c, PAWN) | pieces(c, SHOGI_PAWN, SOLDIER)) & file_bb(s));
}

inline bool Position::can_castle(CastlingRights cr) const {
  return st->castlingRights & cr;
}

inline CastlingRights Position::castling_rights(Color c) const {
  return c & CastlingRights(st->castlingRights);
}

inline bool Position::castling_impeded(CastlingRights cr) const {
  assert(cr == WHITE_OO || cr == WHITE_OOO || cr == BLACK_OO || cr == BLACK_OOO);

  return pieces() & castlingPath[cr];
}

inline Square Position::castling_rook_square(CastlingRights cr) const {
  assert(cr == WHITE_OO || cr == WHITE_OOO || cr == BLACK_OO || cr == BLACK_OOO);

  return castlingRookSquare[cr];
}

// LOA-specific helper – completely private to Position
inline Bitboard Position::dynamic_slider_bb(const std::map<Direction,int>& directions,
                                            Square  sq,
                                            Bitboard blockers,     // pieces that stop us
                                            Bitboard occupiedAll,  // for distance count
                                            Color   c)
{
  Bitboard out = 0;
  for (auto const& [d, limit] : directions)
  {
    if (limit != DYNAMIC_SLIDER_LIMIT) continue;      // not an "x" slider

    Direction step = c == WHITE ?  d : Direction(-d);
    Square    nxt  = sq + step;
    if (!is_ok(nxt) || distance(nxt, nxt - step) > 2) continue; // only rook/bishop steps

    Bitboard line = line_bb(sq, nxt);                 // through board edge
    int dist = popcount(line & occupiedAll);          // how far to travel

    Square dest = sq;
    bool   ok   = true;
    for (int i = 0; i < dist; ++i)
    {
      dest += step;
      if (!is_ok(dest) || distance(dest, dest - step) > 2) { ok = false; break; }
      if (i < dist - 1 && (blockers & dest))       // hit enemy before end
      { ok = false; break; }
    }
    if (ok) out |= square_bb(dest);
  }
  return out;
}

inline Bitboard Position::max_slider_bb(const std::map<Direction,int>& directions,
                                        Square sq,
                                        Bitboard occupied,
                                        Bitboard ownPieces,
                                        Color c,
                                        bool captureMode,
                                        bool includeOwnBlockedAttacks)
{
  Bitboard out = 0;
  for (auto const& [d, limit] : directions)
  {
    if (limit != MAX_SLIDER_LIMIT)
      continue;

    Direction step = c == WHITE ? d : Direction(-d);
    Square dest = SQ_NONE;

    for (Square s2 = sq + step;
         is_ok(s2) && distance(s2, s2 - step) <= 2;
         s2 += step)
    {
      if (occupied & s2)
      {
        if (captureMode && (includeOwnBlockedAttacks || !(ownPieces & s2)))
          dest = s2;
        break;
      }
      dest = s2;
    }
    if (dest != SQ_NONE)
      out |= square_bb(dest);
  }
  return out;
}

inline Bitboard Position::contra_hopper_bb(const std::map<Direction,int>& directions,
                                           Square  sq,
                                           Bitboard occupied,
                                           Bitboard ownPieces,
                                           Color   c,
                                           bool    quietMode,
                                           bool    includeOwnBlockedAttacks)
{
  Bitboard out = 0;
  for (auto const& [d, limit] : directions)
  {
    Square hurdle = sq + (c == WHITE ? d : -d);
    if (!(is_ok(hurdle) && distance(hurdle, sq) <= 2 && (occupied & hurdle)))
        continue;

    int landingDist = 0;
    for (Square s2 = hurdle + (c == WHITE ? d : -d);
         is_ok(s2) && distance(s2, s2 - (c == WHITE ? d : -d)) <= 2;
         s2 += (c == WHITE ? d : -d))
    {
      ++landingDist;
      if (limit && landingDist > limit)
          break;

      const bool blocked = bool(occupied & s2);
      if (quietMode)
      {
        if (blocked)
            break;
        out |= square_bb(s2);
      }
      else
      {
        if (blocked)
        {
          if (includeOwnBlockedAttacks || !(ownPieces & s2))
              out |= square_bb(s2);
          break;
        }
        out |= square_bb(s2);
      }

      if (blocked)
        break;
    }
  }
  return out;
}

inline std::pair<int, int> Position::decode_direction(Direction d) {
  const int raw = int(d);
  int df = raw % int(FILE_NB);

  // Normalize to the shortest file delta representation, matching the
  // previous minimal-Manhattan-distance decode without a brute-force search.
  if (df > int(FILE_NB) / 2)
      df -= int(FILE_NB);
  if (df < -int(FILE_NB) / 2)
      df += int(FILE_NB);

  const int dr = (raw - df) / int(FILE_NB);
  return {dr, df};
}

inline Bitboard Position::wrapped_step_targets(const std::map<Direction, int>& directions,
                                               Square sq, Bitboard occupied,
                                               File maxFile, Rank maxRank,
                                               bool wrapFile, bool wrapRank,
                                               bool requireEmpty) {
  Bitboard out = 0;
  for (const auto& [d, _] : directions)
  {
      auto [dr, df] = decode_direction(d);
      Square to = SQ_NONE;
      if (!wrapped_destination_square(sq, df, dr, maxFile, maxRank, wrapFile, wrapRank, to))
          continue;
      if (requireEmpty && (occupied & to))
          continue;
      out |= to;
  }
  return out;
}

inline Bitboard Position::wrapped_tuple_targets(const std::vector<std::pair<int, int>>& steps,
                                                Color c, Square sq, Bitboard occupied,
                                                File maxFile, Rank maxRank,
                                                bool wrapFile, bool wrapRank,
                                                bool requireEmpty) {
  Bitboard out = 0;
  for (const auto& [dr, df] : steps)
  {
      const int stepR = c == WHITE ? dr : -dr;
      const int stepF = c == WHITE ? df : -df;
      Square to = SQ_NONE;
      if (!wrapped_destination_square(sq, stepF, stepR, maxFile, maxRank, wrapFile, wrapRank, to))
          continue;
      if (requireEmpty && (occupied & to))
          continue;
      out |= to;
  }
  return out;
}

inline Bitboard Position::wrapped_tuple_rider_targets(const std::vector<PieceInfo::TupleRay>& rays,
                                                      Color c, Square sq, Bitboard occupied,
                                                      File maxFile, Rank maxRank,
                                                      bool wrapFile, bool wrapRank,
                                                      bool quietMode) {
  Bitboard out = 0;
  for (const auto& ray : rays)
  {
      const int stepR = c == WHITE ? ray.dr : -ray.dr;
      const int stepF = c == WHITE ? ray.df : -ray.df;
      Square current = sq;
      int count = 0;
      for (;;)
      {
          Square next = SQ_NONE;
          if (!wrapped_destination_square(current, stepF, stepR, maxFile, maxRank, wrapFile, wrapRank, next))
              break;
          if (next == sq)
              break;

          const bool blocked = bool(occupied & next);
          if (!quietMode || !blocked)
              out |= next;

          current = next;
          if (ray.limit > 0 && ++count >= ray.limit)
              break;
          if (blocked)
              break;
      }
  }
  return out;
}

inline Bitboard Position::wrapped_slider_targets(const std::map<Direction, int>& directions,
                                                 Square sq, Bitboard occupied,
                                                 File maxFile, Rank maxRank,
                                                 bool wrapFile, bool wrapRank,
                                                 bool quietMode) {
  Bitboard out = 0;
  for (const auto& [d, limit] : directions)
  {
      auto [dr, df] = decode_direction(d);
      if (!dr && !df)
          continue;

      Square current = sq;
      int steps = 0;
      const int minDistance = slider_min_distance(limit);
      const int maxDistance = slider_max_distance(limit);
      for (;;)
      {
          Square next = SQ_NONE;
          if (!wrapped_destination_square(current, df, dr, maxFile, maxRank, wrapFile, wrapRank, next))
              break;
          if (next == sq)
              break;

          ++steps;
          const bool beyondMin = steps >= minDistance;
          const bool beyondMax = maxDistance > 0 && steps >= maxDistance;
          const bool blocked = bool(occupied & next);

          if (beyondMin)
          {
              if (quietMode)
              {
                  if (!blocked)
                      out |= next;
              }
              else
                  out |= next;
          }

          if (blocked || beyondMax)
              break;
          current = next;
      }
  }
  return out;
}

inline Bitboard Position::wrapped_hopper_targets(const std::map<Direction, int>& directions,
                                                 Square sq, Bitboard occupied,
                                                 File maxFile, Rank maxRank,
                                                 bool wrapFile, bool wrapRank,
                                                 bool quietMode) {
  Bitboard out = 0;
  for (const auto& [d, limit] : directions)
  {
      auto [dr, df] = decode_direction(d);
      if (!dr && !df)
          continue;

      Square current = sq;
      bool hurdle = false;
      int count = 0;
      const int minDistance = slider_min_distance(limit);
      const int maxDistance = slider_max_distance(limit);
      for (;;)
      {
          Square next = SQ_NONE;
          if (!wrapped_destination_square(current, df, dr, maxFile, maxRank, wrapFile, wrapRank, next))
              break;
          if (next == sq)
              break;

          const bool blocked = bool(occupied & next);
          if (hurdle)
          {
              ++count;
              if (count >= minDistance)
              {
                  if (!quietMode || !blocked)
                      out |= next;
              }
              if (maxDistance > 0 && count >= maxDistance)
                  break;
          }

          if (blocked)
          {
              if (!hurdle)
                  hurdle = true;
              else
                  break;
          }
          current = next;
      }
  }
  return out;
}

inline Bitboard Position::wrapped_contra_hopper_targets(const std::map<Direction, int>& directions,
                                                        Color c, Square sq, Bitboard occupied, Bitboard ownPieces,
                                                        File maxFile, Rank maxRank,
                                                        bool wrapFile, bool wrapRank,
                                                        bool quietMode,
                                                        bool includeOwnBlockedAttacks) {
  Bitboard out = 0;
  for (const auto& [d, limit] : directions)
  {
      auto [dr0, df0] = decode_direction(c == WHITE ? d : Direction(-d));
      if (!dr0 && !df0)
          continue;

      Square hurdle = SQ_NONE;
      if (!wrapped_destination_square(sq, df0, dr0, maxFile, maxRank, wrapFile, wrapRank, hurdle))
          continue;
      if (hurdle == sq || !(occupied & hurdle))
          continue;

      int landingDist = 0;
      Square current = hurdle;
      for (;;)
      {
          Square next = SQ_NONE;
          if (!wrapped_destination_square(current, df0, dr0, maxFile, maxRank, wrapFile, wrapRank, next))
              break;
          if (next == sq)
              break;

          ++landingDist;
          if (limit && landingDist > limit)
              break;

          const bool blocked = bool(occupied & next);
          if (quietMode)
          {
              if (blocked)
                  break;
              out |= square_bb(next);
          }
          else
          {
              if (blocked)
              {
                  if (includeOwnBlockedAttacks || !(ownPieces & next))
                      out |= square_bb(next);
                  break;
              }
              out |= square_bb(next);
          }

          if (blocked)
              break;
          current = next;
      }
  }
  return out;
}

inline Bitboard Position::wrapped_bent_rider_targets(bool griffon, Square sq, Bitboard occupied,
                                                     File maxFile, Rank maxRank,
                                                     bool wrapFile, bool wrapRank,
                                                     bool quietMode) {
  Bitboard out = 0;
  auto add_from_pivot = [&](Square pivot, std::initializer_list<Direction> dirs) {
      for (Direction d : dirs)
      {
          std::map<Direction, int> sliderDirs{{d, 0}};
          out |= wrapped_slider_targets(sliderDirs, pivot, occupied, maxFile, maxRank, wrapFile, wrapRank, quietMode);
      }
  };

  if (griffon)
  {
      Square ne = SQ_NONE, nw = SQ_NONE, se = SQ_NONE, sw = SQ_NONE;
      if (wrapped_destination_square(sq, 1, 1, maxFile, maxRank, wrapFile, wrapRank, ne) && ne != sq)
      {
          if (!quietMode || !(occupied & ne))
              out |= ne;
          if (!(occupied & ne))
              add_from_pivot(ne, {EAST, NORTH});
      }
      if (wrapped_destination_square(sq, -1, 1, maxFile, maxRank, wrapFile, wrapRank, nw) && nw != sq)
      {
          if (!quietMode || !(occupied & nw))
              out |= nw;
          if (!(occupied & nw))
              add_from_pivot(nw, {WEST, NORTH});
      }
      if (wrapped_destination_square(sq, 1, -1, maxFile, maxRank, wrapFile, wrapRank, se) && se != sq)
      {
          if (!quietMode || !(occupied & se))
              out |= se;
          if (!(occupied & se))
              add_from_pivot(se, {EAST, SOUTH});
      }
      if (wrapped_destination_square(sq, -1, -1, maxFile, maxRank, wrapFile, wrapRank, sw) && sw != sq)
      {
          if (!quietMode || !(occupied & sw))
              out |= sw;
          if (!(occupied & sw))
              add_from_pivot(sw, {WEST, SOUTH});
      }
  }
  else
  {
      Square n = SQ_NONE, w = SQ_NONE, e = SQ_NONE, s = SQ_NONE;
      if (wrapped_destination_square(sq, 0, 1, maxFile, maxRank, wrapFile, wrapRank, n) && n != sq)
      {
          if (!quietMode || !(occupied & n))
              out |= n;
          if (!(occupied & n))
              add_from_pivot(n, {NORTH_EAST, NORTH_WEST});
      }
      if (wrapped_destination_square(sq, -1, 0, maxFile, maxRank, wrapFile, wrapRank, w) && w != sq)
      {
          if (!quietMode || !(occupied & w))
              out |= w;
          if (!(occupied & w))
              add_from_pivot(w, {NORTH_WEST, SOUTH_WEST});
      }
      if (wrapped_destination_square(sq, 1, 0, maxFile, maxRank, wrapFile, wrapRank, e) && e != sq)
      {
          if (!quietMode || !(occupied & e))
              out |= e;
          if (!(occupied & e))
              add_from_pivot(e, {NORTH_EAST, SOUTH_EAST});
      }
      if (wrapped_destination_square(sq, 0, -1, maxFile, maxRank, wrapFile, wrapRank, s) && s != sq)
      {
          if (!quietMode || !(occupied & s))
              out |= s;
          if (!(occupied & s))
              add_from_pivot(s, {SOUTH_EAST, SOUTH_WEST});
      }
  }

  return out;
}

inline Bitboard Position::wrapped_leap_rider_targets(const std::map<Direction, int>& directions,
                                                     Color c, Square sq, Bitboard occupied,
                                                     File maxFile, Rank maxRank,
                                                     bool wrapFile, bool wrapRank,
                                                     bool quietMode) {
  Bitboard out = 0;
  for (const auto& [d, limit] : directions)
  {
      auto [dr, df] = decode_direction(c == WHITE ? d : Direction(-d));
      if (!dr && !df)
          continue;

      Square current = sq;
      int count = 0;
      for (;;)
      {
          Square next = SQ_NONE;
          if (!wrapped_destination_square(current, df, dr, maxFile, maxRank, wrapFile, wrapRank, next))
              break;
          if (next == sq)
              break;

          const bool blocked = bool(occupied & next);
          if (!quietMode || !blocked)
              out |= next;

          if (limit > 0 && ++count >= limit)
              break;
          if (blocked)
              break;
          current = next;
      }
  }
  return out;
}

inline Bitboard Position::wrapped_rose_targets(Square from, Bitboard occupied,
                                               File maxFile, Rank maxRank,
                                               bool wrapFile, bool wrapRank,
                                               bool quietMode) {
  Bitboard attack = 0;

  for (int start = 0; start < 8; ++start)
      for (int turn : {-1, 1})
      {
          Square current = from;
          int index = start;
          for (int leg = 0; leg < 7; ++leg)
          {
              Square to = SQ_NONE;
              if (!wrapped_destination_square(current,
                                              RoseSteps[index].second,
                                              RoseSteps[index].first,
                                              maxFile, maxRank, wrapFile, wrapRank, to))
                  break;
              if (to == from)
                  break;
              if (!quietMode || !(occupied & to))
                  attack |= to;
              if (occupied & to)
                  break;
              current = to;
              index = (index + turn + 8) % 8;
          }
      }

  return attack;
}

inline Bitboard Position::special_rider_bb(const PieceInfo* pi, MoveModality modality,
                                           Square sq, Bitboard occupied,
                                           Bitboard occupiedAll, Bitboard ownPieces,
                                           Color c, bool captureMode,
                                           bool includeOwnBlockedAttacks)
{
  const uint8_t augment = pi->riderAugmentMask;
  if (augment == PieceInfo::AUGMENT_NONE)
      return Bitboard(0);
  Bitboard b = 0;
  if (augment & PieceInfo::AUGMENT_DYNAMIC)
      b |= Position::dynamic_slider_bb(pi->slider[0][modality], sq, occupied, occupiedAll, c);
  if (augment & PieceInfo::AUGMENT_MAX)
      b |= Position::max_slider_bb(pi->slider[0][modality], sq, occupied, ownPieces, c, captureMode, includeOwnBlockedAttacks);
  if (augment & PieceInfo::AUGMENT_CONTRA)
      b |= Position::contra_hopper_bb(pi->contraHopper[0][modality], sq, occupied, ownPieces, c, !captureMode, includeOwnBlockedAttacks);
  return b;
}

inline Bitboard Position::attacks_from(Color c, PieceType pt, Square s) const {
  assert(pt != NO_PIECE_TYPE);
  Bitboard occupancy = byTypeBB[ALL_PIECES];
  if (const SpellContext* spellCtx = current_spell_context(); spellCtx && c == sideToMove)
      occupancy &= ~spellCtx->jumpRemoved;
  return attacks_from(c, pt, s, occupancy);
}

inline Bitboard Position::attacks_from(Color c, PieceType pt, Square s, Bitboard occupancy) const {
  assert(pt != NO_PIECE_TYPE);

  if (topology_wraps())
  {
      PieceType movePt = pt == KING ? king_type() : pt;
      const PieceInfo* pi = pieceMap.get(movePt);
      const bool wrapFile = wraps_files();
      const bool wrapRank = wraps_ranks();
      Bitboard b = 0;

      if (pt == PAWN)
      {
          const int forward = c == WHITE ? 1 : -1;
          Square to = SQ_NONE;
          if (wrapped_destination_square(s, -1, forward, max_file(), max_rank(), wrapFile, wrapRank, to))
              b |= to;
          if (wrapped_destination_square(s, 1, forward, max_file(), max_rank(), wrapFile, wrapRank, to))
              b |= to;
          return b & board_bb(c, pt);
      }

      b |= wrapped_step_targets(pi->steps[0][MODALITY_CAPTURE], s, occupancy, max_file(), max_rank(), wrapFile, wrapRank, false);
      b |= wrapped_tuple_targets(pi->tupleSteps[0][MODALITY_CAPTURE], c, s, occupancy, max_file(), max_rank(), wrapFile, wrapRank, false);
      b |= wrapped_tuple_rider_targets(pi->tupleSlider[0][MODALITY_CAPTURE], c, s, occupancy, max_file(), max_rank(), wrapFile, wrapRank, false);
      b |= wrapped_slider_targets(pi->slider[0][MODALITY_CAPTURE], s, occupancy, max_file(), max_rank(), wrapFile, wrapRank, false);
      b |= wrapped_hopper_targets(pi->hopper[0][MODALITY_CAPTURE], s, occupancy, max_file(), max_rank(), wrapFile, wrapRank, false);
      b |= wrapped_contra_hopper_targets(pi->contraHopper[0][MODALITY_CAPTURE], c, s, occupancy, pieces(c), max_file(), max_rank(), wrapFile, wrapRank, false, true);
      if (pi->griffon[0][MODALITY_CAPTURE])
          b |= wrapped_bent_rider_targets(true, s, occupancy, max_file(), max_rank(), wrapFile, wrapRank, false);
      if (pi->manticore[0][MODALITY_CAPTURE])
          b |= wrapped_bent_rider_targets(false, s, occupancy, max_file(), max_rank(), wrapFile, wrapRank, false);
      b |= wrapped_leap_rider_targets(pi->leapRider[0][MODALITY_CAPTURE], c, s, occupancy, max_file(), max_rank(), wrapFile, wrapRank, false);
      if (pi->rose[0][MODALITY_CAPTURE])
          b |= wrapped_rose_targets(s, occupancy, max_file(), max_rank(), wrapFile, wrapRank, false);

      if (pi->friendlyJump)
          b &= ~pieces(c);
      return b & board_bb(c, pt);
  }

  if (fast_attacks() && (pt != KING || king_type() == KING))
  {
      Bitboard b = 0;
      switch (pt)
      {
      case PAWN:
          b = pawn_attacks_bb(c, s);
          break;
      case KNIGHT:
          b = attacks_bb<KNIGHT>(s);
          break;
      case BISHOP:
          b = attacks_bb<BISHOP>(s, occupancy);
          break;
      case ROOK:
          b = attacks_bb<ROOK>(s, occupancy);
          break;
      case QUEEN:
          b = attacks_bb<BISHOP>(s, occupancy) | attacks_bb<ROOK>(s, occupancy);
          break;
      case KING:
      case COMMONER:
          b = attacks_bb<KING>(s);
          break;
      case ARCHBISHOP:
          b = attacks_bb<BISHOP>(s, occupancy) | attacks_bb<KNIGHT>(s);
          break;
      case CHANCELLOR:
          b = attacks_bb<ROOK>(s, occupancy) | attacks_bb<KNIGHT>(s);
          break;
      case IMMOBILE_PIECE:
          b = Bitboard(0);
          break;
      default:
          b = attacks_bb(c, pt, s, occupancy);
          break;
      }
      return b & board_bb();
  }

  if (fast_attacks2() && (pt != KING || king_type() == KING))
      return attacks_bb(c, pt, s, occupancy) & board_bb();

  PieceType movePt = pt == KING ? king_type() : pt;
  const PieceInfo* pi = pieceMap.get(movePt);

  if ((fast_attacks() || fast_attacks2()) && pi->riderAugmentMask == PieceInfo::AUGMENT_NONE)
      return attacks_bb(c, movePt, s, occupancy) & board_bb();

  if (pi->friendlyJump)
      occupancy &= ~pieces(c);

  Bitboard b = attacks_bb(c, movePt, s, occupancy);

  b |= Position::special_rider_bb(pi, MODALITY_CAPTURE, s, occupancy, occupancy, pieces(c), c, true, true);

  if (pi->friendlyJump)
      b &= ~pieces(c);          // never hit our own men
  // Xiangqi soldier
  if (pt == SOLDIER && !(promoted_soldiers(c) & s))
      b &= file_bb(file_of(s));
  // Janggi cannon restrictions
  if (pt == JANGGI_CANNON)
  {
      b &= ~pieces(pt);
      b &= attacks_bb(c, pt, s, (occupancy ^ pieces(pt)));
  }
  // Janggi palace moves
  if (diagonal_lines() & s)
  {
      PieceType diagType = movePt == WAZIR ? FERS : movePt == SOLDIER ? PAWN : movePt == ROOK ? BISHOP : NO_PIECE_TYPE;
      if (diagType)
          b |= attacks_bb(c, diagType, s, occupancy) & diagonal_lines();
      else if (movePt == JANGGI_CANNON)
          b |=  rider_attacks_bb<RIDER_CANNON_DIAG>(s, occupancy)
              & rider_attacks_bb<RIDER_CANNON_DIAG>(s, (occupancy ^ pieces(pt)))
              & ~pieces(pt)
              & diagonal_lines();
  }
  return b & board_bb(c, pt);
}

inline Bitboard Position::moves_from(Color c, PieceType pt, Square s) const {
    assert(pt != NO_PIECE_TYPE);

    Bitboard extraDestinations = 0x00;

    if (topology_wraps())
    {
        Bitboard occupancy = byTypeBB[ALL_PIECES];
        if (const SpellContext* spellCtx = current_spell_context(); spellCtx && c == sideToMove)
            occupancy &= ~spellCtx->jumpRemoved;

        PieceType movePt = pt == KING ? king_type() : pt;
        const PieceInfo* pi = pieceMap.get(movePt);
        const bool wrapFile = wraps_files();
        const bool wrapRank = wraps_ranks();

        if (pt == PAWN)
        {
            Bitboard b = 0;
            const int forward = c == WHITE ? 1 : -1;
            Square to = SQ_NONE;
            if (wrapped_destination_square(s, 0, forward, max_file(), max_rank(), wrapFile, wrapRank, to) && !(occupancy & to))
            {
                b |= to;
                if ((double_step_region(c, pt) & s)
                    && wrapped_destination_square(to, 0, forward, max_file(), max_rank(), wrapFile, wrapRank, to)
                    && !(occupancy & to))
                    b |= to;
            }
            if ((triple_step_region(c, pt) & s))
            {
                Square s1 = SQ_NONE, s2 = SQ_NONE, s3 = SQ_NONE;
                if (wrapped_destination_square(s, 0, forward, max_file(), max_rank(), wrapFile, wrapRank, s1)
                    && !(occupancy & s1)
                    && wrapped_destination_square(s1, 0, forward, max_file(), max_rank(), wrapFile, wrapRank, s2)
                    && !(occupancy & s2)
                    && wrapped_destination_square(s2, 0, forward, max_file(), max_rank(), wrapFile, wrapRank, s3)
                    && !(occupancy & s3))
                    b |= s1 | s2 | s3;
            }
            return b & board_bb(c, pt);
        }

        Bitboard b = 0;
        b |= wrapped_step_targets(pi->steps[0][MODALITY_QUIET], s, occupancy, max_file(), max_rank(), wrapFile, wrapRank, true);
        b |= wrapped_tuple_targets(pi->tupleSteps[0][MODALITY_QUIET], c, s, occupancy, max_file(), max_rank(), wrapFile, wrapRank, true);
        b |= wrapped_tuple_rider_targets(pi->tupleSlider[0][MODALITY_QUIET], c, s, occupancy, max_file(), max_rank(), wrapFile, wrapRank, true);
        b |= wrapped_slider_targets(pi->slider[0][MODALITY_QUIET], s, occupancy, max_file(), max_rank(), wrapFile, wrapRank, true);
        b |= wrapped_hopper_targets(pi->hopper[0][MODALITY_QUIET], s, occupancy, max_file(), max_rank(), wrapFile, wrapRank, true);
        b |= wrapped_contra_hopper_targets(pi->contraHopper[0][MODALITY_QUIET], c, s, occupancy, pieces(c), max_file(), max_rank(), wrapFile, wrapRank, true, false);
        if (pi->griffon[0][MODALITY_QUIET])
            b |= wrapped_bent_rider_targets(true, s, occupancy, max_file(), max_rank(), wrapFile, wrapRank, true);
        if (pi->manticore[0][MODALITY_QUIET])
            b |= wrapped_bent_rider_targets(false, s, occupancy, max_file(), max_rank(), wrapFile, wrapRank, true);
        b |= wrapped_leap_rider_targets(pi->leapRider[0][MODALITY_QUIET], c, s, occupancy, max_file(), max_rank(), wrapFile, wrapRank, true);
        if (pi->rose[0][MODALITY_QUIET])
            b |= wrapped_rose_targets(s, occupancy, max_file(), max_rank(), wrapFile, wrapRank, true);

        if (double_step_region(c, pt) & s)
        {
            b |= wrapped_step_targets(pi->steps[1][MODALITY_QUIET], s, occupancy, max_file(), max_rank(), wrapFile, wrapRank, true);
            b |= wrapped_tuple_targets(pi->tupleSteps[1][MODALITY_QUIET], c, s, occupancy, max_file(), max_rank(), wrapFile, wrapRank, true);
            b |= wrapped_tuple_rider_targets(pi->tupleSlider[1][MODALITY_QUIET], c, s, occupancy, max_file(), max_rank(), wrapFile, wrapRank, true);
            b |= wrapped_slider_targets(pi->slider[1][MODALITY_QUIET], s, occupancy, max_file(), max_rank(), wrapFile, wrapRank, true);
            b |= wrapped_hopper_targets(pi->hopper[1][MODALITY_QUIET], s, occupancy, max_file(), max_rank(), wrapFile, wrapRank, true);
            b |= wrapped_contra_hopper_targets(pi->contraHopper[1][MODALITY_QUIET], c, s, occupancy, pieces(c), max_file(), max_rank(), wrapFile, wrapRank, true, false);
            if (pi->griffon[1][MODALITY_QUIET])
                b |= wrapped_bent_rider_targets(true, s, occupancy, max_file(), max_rank(), wrapFile, wrapRank, true);
            if (pi->manticore[1][MODALITY_QUIET])
                b |= wrapped_bent_rider_targets(false, s, occupancy, max_file(), max_rank(), wrapFile, wrapRank, true);
            b |= wrapped_leap_rider_targets(pi->leapRider[1][MODALITY_QUIET], c, s, occupancy, max_file(), max_rank(), wrapFile, wrapRank, true);
            if (pi->rose[1][MODALITY_QUIET])
                b |= wrapped_rose_targets(s, occupancy, max_file(), max_rank(), wrapFile, wrapRank, true);
        }

        if (pi->friendlyJump)
            b &= ~pieces(c);
        return b & board_bb(c, pt);
    }

    // Piece specific double/triple step region
    // It adds new moves to the pieces, enabling the piece to move 2 or 3 squares ahead
    // Since double step in introduced from chess variants where pawns cannot capture forward, capturing moves are not included here.
    // Double/Triple step cannot attack other pieces, so attacks_from(Color c, PieceType pt, Square s) is not changed
    // Due to some unknown issues, shift<Direction D>(Bitboard b) cannot be used here
    if (var->pieceSpecificTripleStepRegion)
    {
        Bitboard tripleStepRegion = this->triple_step_region(c, pt);
        Bitboard occupied = this->pieces();  //Bitboard where the bits whose corresponding squares having a piece on it are 1
        Bitboard piecePosition = square_bb(s);  //Bitboard where only the bit which refers to the square that the piece starts the move (original square) is 1
        Bitboard extraMultipleStepMoveDestinations = 0x00;  //Bitboard where extra legal multi-step destination square bits are 1
        if (tripleStepRegion & piecePosition & this->not_moved_pieces(c))  //If the original square is in tripleStepRegion and the piece is not moved
        {
            Bitboard oneSquareAhead = (c == WHITE) ? piecePosition << NORTH : piecePosition >> NORTH;
            if (!(oneSquareAhead & occupied))  //If the square which is 1 square ahead of original square is NOT blocked
            {
                extraMultipleStepMoveDestinations |= oneSquareAhead;  //Add the square which is 1 square ahead of original square to destination squares for triple step
                Bitboard twoSquareAhead = (c == WHITE) ? piecePosition << NORTH << NORTH : piecePosition >> NORTH >> NORTH;
                if (!(twoSquareAhead & occupied))  //If the square which is 2 squares ahead of original square is NOT blocked
                {
                    extraMultipleStepMoveDestinations |= twoSquareAhead;  //Add the square which is 2 squares ahead of original square to destination squares for triple step
                    Bitboard threeSquareAhead = (c == WHITE) ? piecePosition << NORTH << NORTH << NORTH : piecePosition >> NORTH >> NORTH >> NORTH;
                    if (!(threeSquareAhead & occupied))  //If the square which is 3 squares ahead of original square is NOT blocked
                    {
                        extraMultipleStepMoveDestinations |= threeSquareAhead;  //Add the square which is 3 squares ahead of original square to destination squares for triple step
                    }
                }
            }
            extraDestinations |= extraMultipleStepMoveDestinations; //Add destination squares to base board
        }
    }
    if (var->pieceSpecificDoubleStepRegion)
    {
        Bitboard doubleStepRegion = this->double_step_region(c, pt);
        Bitboard occupied = this->pieces();  //Bitboard where the bits whose corresponding squares having a piece on it are 1
        Bitboard piecePosition = square_bb(s);  //Bitboard where only the bit which refers to the square that the piece starts the move (original square) is 1
        Bitboard extraMultipleStepMoveDestinations = 0x00;  //Bitboard where extra legal multi-step destination square bits are 1
        if (doubleStepRegion & piecePosition & this->not_moved_pieces(c))  //If the original square is in doubleStepRegion and the piece is not moved
        {
            Bitboard oneSquareAhead = (c == WHITE) ? piecePosition << NORTH : piecePosition >> NORTH;
            if (!(oneSquareAhead & occupied))  //If the square which is 1 square ahead of original square is NOT blocked
            {
                extraMultipleStepMoveDestinations |= oneSquareAhead;  //Add the square which is 1 square ahead of original square to destination squares for triple step
                Bitboard twoSquareAhead = (c == WHITE) ? piecePosition << NORTH << NORTH : piecePosition >> NORTH >> NORTH;
                if (!(twoSquareAhead & occupied))  //If the square which is 2 squares ahead of original square is NOT blocked
                {
                    extraMultipleStepMoveDestinations |= twoSquareAhead;  //Add the square which is 2 squares ahead of original square to destination squares for triple step
                }
            }
            extraDestinations |= extraMultipleStepMoveDestinations; //Add destination squares to base board
        }
    }

  Bitboard occupancy = byTypeBB[ALL_PIECES];
  if (const SpellContext* spellCtx = current_spell_context(); spellCtx && c == sideToMove)
      occupancy &= ~spellCtx->jumpRemoved;

  if ((fast_attacks() || fast_attacks2()) && (pt != KING || king_type() == KING))
      return (moves_bb(c, pt, s, occupancy) | extraDestinations) & board_bb();

  PieceType movePt = pt == KING ? king_type() : pt;
  const PieceInfo* pi = pieceMap.get(movePt);

  if ((fast_attacks() || fast_attacks2()) && pi->riderAugmentMask == PieceInfo::AUGMENT_NONE)
      return (moves_bb(c, movePt, s, occupancy) | extraDestinations) & board_bb();

  if (pi->friendlyJump)
      occupancy &= ~pieces(c);

  Bitboard b = (moves_bb(c, movePt, s, occupancy) | extraDestinations);

  b |= Position::special_rider_bb(pi, MODALITY_QUIET, s, occupancy, byTypeBB[ALL_PIECES], pieces(c), c, false, false);

  if (pi->friendlyJump)
      b &= ~pieces(c);          // cannot land on own piece
  // Add initial moves
  if (double_step_region(c, pt) & s)
  {
      b |= moves_bb<true>(c, movePt, s, occupancy);
      b |= Position::special_rider_bb(pi, MODALITY_QUIET, s, occupancy, byTypeBB[ALL_PIECES], pieces(c), c, true, false);
  }
  // Xiangqi soldier
  if (pt == SOLDIER && !(promoted_soldiers(c) & s))
      b &= file_bb(file_of(s));
  // Janggi cannon restrictions
  if (pt == JANGGI_CANNON)
  {
      b &= ~pieces(pt);
      b &= attacks_bb(c, pt, s, (occupancy ^ pieces(pt)));
  }
  // Janggi palace moves
  if (diagonal_lines() & s)
  {
      PieceType diagType = movePt == WAZIR ? FERS : movePt == SOLDIER ? PAWN : movePt == ROOK ? BISHOP : NO_PIECE_TYPE;
      if (diagType)
          b |= attacks_bb(c, diagType, s, occupancy) & diagonal_lines();
      else if (movePt == JANGGI_CANNON)
          b |=  rider_attacks_bb<RIDER_CANNON_DIAG>(s, occupancy)
              & rider_attacks_bb<RIDER_CANNON_DIAG>(s, (occupancy ^ pieces(pt)))
              & ~pieces(pt)
              & diagonal_lines();
  }
  return b & board_bb(c, pt);
}

inline Bitboard Position::push_targets_from(Color c, PieceType pt, Square s) const {
  return (PseudoAttacks[c][pt][s] | PseudoMoves[0][c][pt][s]) & board_bb(c, pt);
}

inline Bitboard Position::attackers_to(Square s) const {
  return attackers_to(s, pieces());
}

inline Bitboard Position::attackers_to(Square s, Color c) const {
  return attackers_to(s, byTypeBB[ALL_PIECES], c);
}

inline Bitboard Position::attackers_to(Square s, Bitboard occupied, Color c) const {
  return attackers_to(s, occupied, c, byTypeBB[JANGGI_CANNON]);
}

inline Bitboard Position::attackers_to_king(Square s, Color c) const {
  return attackers_to_king(s, byTypeBB[ALL_PIECES], c);
}

inline Bitboard Position::attackers_to_king(Square s, Bitboard occupied, Color c) const {
  return attackers_to_king(s, occupied, c, byTypeBB[JANGGI_CANNON]);
}

inline Bitboard Position::checkers() const {
  return st->checkersBB;
}

inline Bitboard Position::evasion_checkers() const {
  return st->evasionCheckersBB;
}

inline Bitboard Position::passive_blast_checkers(Color victim, Bitboard occupied) const {
  if (!var->blastPassiveTypes || !count<KING>(victim))
      return Bitboard(0);

  Square ksq = square<KING>(victim);
  if (blast_immune_bb() & square_bb(ksq))
      return Bitboard(0);

  Bitboard burners = Bitboard(0);
  for (PieceType pt = PAWN; pt < PIECE_TYPE_NB; ++pt)
      if (var->blastPassiveTypes & pt)
          burners |= pieces(~victim, pt);

  return blast_pattern(ksq) & burners & occupied;
}

inline Bitboard Position::blockers_for_king(Color c) const {
  return st->blockersForKing[c];
}

inline Bitboard Position::pinners(Color c) const {
  return st->pinners[c];
}

inline Bitboard Position::check_squares(PieceType pt) const {
  return st->checkSquares[pt];
}

inline bool Position::pawn_passed(Color c, Square s) const {
  return !(pieces(~c, PAWN) & passed_pawn_span(c, s));
}

inline int Position::pawns_on_same_color_squares(Color c, Square s) const {
  return popcount(pieces(c, PAWN) & ((DarkSquares & s) ? DarkSquares : ~DarkSquares));
}

inline Key Position::key() const {
  return st->rule50 < 14 ? st->key
                         : st->key ^ make_key((st->rule50 - 14) / 8);
}

inline Key Position::pawn_key() const {
  return st->pawnKey;
}

inline Score Position::psq_score() const {
  return psq;
}

inline Value Position::non_pawn_material(Color c) const {
  return st->nonPawnMaterial[c];
}

inline Value Position::non_pawn_material() const {
  return non_pawn_material(WHITE) + non_pawn_material(BLACK);
}

inline int Position::game_ply() const {
  return gamePly;
}

inline int Position::board_honor_counting_ply(int countStarted) const {
  return countStarted == 0 ?
      st->countingPly :
      countStarted < 0 ? 0 : std::max(1 + gamePly - countStarted, 0);
}

inline bool Position::board_honor_counting_shorter(int countStarted) const {
  return counting_rule() == CAMBODIAN_COUNTING && 126 - board_honor_counting_ply(countStarted) < st->countingLimit - st->countingPly;
}

inline int Position::counting_limit(int countStarted) const {
  return board_honor_counting_shorter(countStarted) ? 126 : st->countingLimit;
}

inline int Position::counting_ply(int countStarted) const {
  return !count<PAWN>() && (count<ALL_PIECES>(WHITE) <= 1 || count<ALL_PIECES>(BLACK) <= 1) && !board_honor_counting_shorter(countStarted) ?
      st->countingPly :
      board_honor_counting_ply(countStarted);
}

inline int Position::rule50_count() const {
  return st->rule50;
}

inline bool Position::opposite_bishops() const {
  return   count<BISHOP>(WHITE) == 1
        && count<BISHOP>(BLACK) == 1
        && opposite_colors(square<BISHOP>(WHITE), square<BISHOP>(BLACK));
}

inline bool Position::is_promoted(Square s) const {
  return promotedPieces & s;
}

inline bool Position::is_chess960() const {
  return chess960;
}

inline bool Position::capture_or_promotion(Move m) const {
  assert(is_ok(m));
  return type_of(m) == PROMOTION || capture(m);
}

inline Square Position::jump_capture_square(Square from, Square to) const {
  assert(is_ok(from));
  assert(is_ok(to));

  Piece mover = piece_on(from);
  PieceSet jumpTypes = jump_capture_types();
  if (mover == NO_PIECE || (!(jumpTypes & ALL_PIECES) && !(jumpTypes & type_of(mover))) || !empty(to))
      return SQ_NONE;

  Square mid = JumpMidpoint[from][to];
  if (mid == SQ_NONE)
      return SQ_NONE;
  Piece jumped = piece_on(mid);
  if (jumped == NO_PIECE || (color_of(jumped) == color_of(mover) && !self_capture(type_of(mover))))
      return SQ_NONE;

  return mid;
}

inline bool Position::is_jump_capture(Move m) const {
  assert(is_ok(m));
  return (type_of(m) == NORMAL || type_of(m) == PROMOTION) && jump_capture_square(from_sq(m), to_sq(m)) != SQ_NONE;
}

inline bool Position::capture(Move m) const {
  assert(is_ok(m));
  if (type_of(m) == EN_PASSANT)
      return true;
  if (type_of(m) == PULL || type_of(m) == SWAP)
      return false;
  if (type_of(m) == CASTLING || from_sq(m) == to_sq(m))
      return false;
  if (push_move(m))
      return push_captures(m);

  if (type_of(m) == NORMAL || type_of(m) == PROMOTION)
  {
      Piece mover = moved_piece(m);
      PieceSet jumpTypes = jump_capture_types();
      if (mover != NO_PIECE && ((jumpTypes & ALL_PIECES) || (jumpTypes & type_of(mover))))
      {
          if (jump_capture_square(from_sq(m), to_sq(m)) != SQ_NONE)
              return true;
      }
  }

  Square to = to_sq(m);
  return !empty(to) || bool(st->deadSquares & to);
}

inline Square Position::capture_square(Square to) const {
  assert(is_ok(to));
  // The capture square of en passant is either the marked ep piece or the closest piece behind the target square
  Bitboard customEp = ep_squares() & pieces();
  if (customEp)
  {
      // For longer custom en passant paths, we take the frontmost piece
      return sideToMove == WHITE ? lsb(customEp) : msb(customEp);
  }
  else
  {
      if (topology_wraps())
      {
          Square s = to;
          int backwardDr = sideToMove == WHITE ? -1 : 1;
          for (int i = 0; i < ranks(); ++i)
          {
              Square next;
              if (!wrapped_destination_square(s, 0, backwardDr, max_file(), max_rank(), wraps_files(), wraps_ranks(), next))
                  break;
              s = next;
              if (pieces(~sideToMove) & s)
                  return s;
          }
      }
      // The capture square of normal en passant is the closest piece behind the target square
      Bitboard epCandidates = pieces(~sideToMove) & forward_file_bb(~sideToMove, to);
      return sideToMove == WHITE ? msb(epCandidates) : lsb(epCandidates);
  }
}

inline Square Position::capture_square(Move m) const {
  Square to = to_sq(m);
  return type_of(m) == EN_PASSANT ? capture_square(to)
       : is_jump_capture(m)      ? jump_capture_square(from_sq(m), to)
       : push_move(m)            ? push_capture_square(m)
                                 : to;
}

inline bool Position::paired_drop(Move m) const {
  return type_of(m) == DROP2 || (is_gating(m) && (symmetric_drop_types() & dropped_piece_type(m)));
}

inline Square Position::secondary_drop_square(Move m) const {
  return paired_drop(m) ? (type_of(m) == DROP2 ? from_sq(m) : mirrored_pair_drop_square(gating_square(m))) : SQ_NONE;
}

inline Square Position::mirrored_pair_drop_square(Square s) const {
  int f = int(file_of(s));
  int files = int(max_file()) + 1;
  int mirrored = files - 1 - f;

  if ((files & 1) && mirrored == f)
      mirrored = std::min(files - 1, f + 1);

  return make_square(File(mirrored), rank_of(s));
}

inline bool Position::virtual_drop(Move m) const {
  assert(is_ok(m));
  return type_of(m) == DROP && !can_drop(side_to_move(), in_hand_piece_type(m)) && exchange_piece(m) == NO_PIECE_TYPE;
}

inline Piece Position::captured_piece() const {
  return st->captured.piece;
}

inline Bitboard Position::fog_area() const {
  Bitboard b = board_bb();
  // Our own pieces are visible
  Bitboard visible = pieces(sideToMove);
  // Squares where we can move to are visible as well
  for (const auto& m : MoveList<LEGAL>(*this))
  {
    Square to = to_sq(m);
    visible |= to;
  }
  // Everything else is invisible
  return ~visible & b;
}

inline Piece Position::captured_piece(Move m) const {
  return capture(m) ? piece_on(capture_square(m)) : NO_PIECE;
}

inline const std::string Position::piece_to_partner() const {
  if (!st->captured.piece) return std::string();
  Color color = color_of(st->captured.piece);
  Piece piece = st->captured.promoted ?
      (st->captured.unpromoted ? st->captured.unpromoted : make_piece(color, main_promotion_pawn_type(color))) :
      st->captured.piece;
  return piece_symbol(piece);
}

inline Thread* Position::this_thread() const {
  return thisThread;
}

inline void Position::put_piece(Piece pc, Square s, bool isPromoted, Piece unpromotedPc, bool markNotMoved) {

  board[s] = pc;
  byTypeBB[ALL_PIECES] |= byTypeBB[type_of(pc)] |= s;
  byColorBB[color_of(pc)] |= s;
  pieceCount[pc]++;
  pieceCount[make_piece(color_of(pc), ALL_PIECES)]++;
  psq += PSQT::psq[pc][s];
  if (isPromoted)
      promotedPieces |= s;
  unpromotedBoard[s] = unpromotedPc;
  if (extinction_must_appear() & piece_set(type_of(pc)))
      st->extinctionSeen[color_of(pc)] |= piece_set(type_of(pc));

  if (markNotMoved)
      this->st->not_moved_pieces[color_of(pc)] |= square_bb(s);
}

inline void Position::remove_piece(Square s) {

  Piece pc = board[s];
  byTypeBB[ALL_PIECES] ^= s;
  byTypeBB[type_of(pc)] ^= s;
  byColorBB[color_of(pc)] ^= s;
  board[s] = NO_PIECE;
  pieceCount[pc]--;
  pieceCount[make_piece(color_of(pc), ALL_PIECES)]--;
  psq -= PSQT::psq[pc][s];
  promotedPieces -= s;
  unpromotedBoard[s] = NO_PIECE;

  //not-moved-piece bitboard must ensure that there is a piece
  this->st->not_moved_pieces[WHITE] &= (~square_bb(s));
  this->st->not_moved_pieces[BLACK] &= (~square_bb(s));
}

inline bool Position::is_initial_pawn(Piece pc, Square s) const {
  return type_of(pc) == PAWN && rank_of(s) == relative_rank(color_of(pc), RANK_2, max_rank());
}

inline void Position::move_piece(Square from, Square to) {

  Piece pc = board[from];
  Bitboard fromTo = square_bb(from) ^ to; // from == to needs to cancel out
  byTypeBB[ALL_PIECES] ^= fromTo;
  byTypeBB[type_of(pc)] ^= fromTo;
  byColorBB[color_of(pc)] ^= fromTo;
  board[from] = NO_PIECE;
  board[to] = pc;
  psq += PSQT::psq[pc][to] - PSQT::psq[pc][from];
  if (is_promoted(from))
      promotedPieces ^= fromTo;
  unpromotedBoard[to] = unpromotedBoard[from];
  unpromotedBoard[from] = NO_PIECE;

  //Once moved, no matter whether the piece is on original square or on destination square (including captures) or the color of the piece, it is no longer not-moved-piece
  this->st->not_moved_pieces[WHITE] &= (~(square_bb(from) | square_bb(to)));
  this->st->not_moved_pieces[BLACK] &= (~(square_bb(from) | square_bb(to)));
}

inline void Position::swap_piece(Square from, Square to) {
  Piece fromPc = piece_on(from);
  Piece toPc = piece_on(to);
  bool fromPromoted = is_promoted(from);
  bool toPromoted = is_promoted(to);
  Piece fromUnpromoted = fromPromoted ? unpromoted_piece_on(from) : NO_PIECE;
  Piece toUnpromoted = toPromoted ? unpromoted_piece_on(to) : NO_PIECE;

  remove_piece(from);
  remove_piece(to);
  put_piece(toPc, from, toPromoted, toUnpromoted);
  put_piece(fromPc, to, fromPromoted, fromUnpromoted);
}

inline void Position::do_move(Move m, StateInfo& newSt) {
  do_move(m, newSt, gives_check(m));
}

inline StateInfo* Position::state() const {

  return st;
}

// Variant-specific

inline int Position::count_in_hand(PieceType pt) const {
  return pieceCountInHand[WHITE][pt] + pieceCountInHand[BLACK][pt];
}

inline int Position::count_in_hand(Color c, PieceType pt) const {
  return pieceCountInHand[c][pt];
}

inline int Position::count_with_hand(Color c, PieceType pt) const {
  return pieceCount[make_piece(c, pt)] + pieceCountInHand[c][pt];
}

inline int Position::count_in_prison(Color c, PieceType pt) const {
  return pieceCountInPrison[c][pt];
}

inline bool Position::prison_pawn_promotion() const {
  return var->prisonPawnPromotion;
}

inline bool Position::bikjang() const {
  return st->bikjang;
}

inline bool Position::allow_virtual_drop(Color c, PieceType pt) const {
  assert(two_boards());
  if (!virtual_drops())
      return false;
  if (var->virtualDropLimitEnabled)
      return pt != KING && var->virtualDropLimit[pt] > 0
          && count_in_hand(c, pt) >= -var->virtualDropLimit[pt];
  // Do we allow a virtual drop?
  return pt != KING && (   count_in_hand(c, PAWN) >= -(pt == PAWN)
                        && count_in_hand(c, KNIGHT) >= -(pt == PAWN)
                        && count_in_hand(c, BISHOP) >= -(pt == PAWN)
                        && count_in_hand(c, ROOK) >= 0
                        && count_in_hand(c, QUEEN) >= 0);
}

inline bool Position::virtual_drops() const {
  return var->virtualDrops;
}

inline Value Position::material_counting_result() const {
  auto weight_count = [this](PieceType pt, int v){ return v * (count(WHITE, pt) - count(BLACK, pt)); };
  int materialCount;
  Value result;
  switch (var->materialCounting)
  {
  case JANGGI_MATERIAL:
      materialCount =  weight_count(ROOK, 13)
                     + weight_count(JANGGI_CANNON, 7)
                     + weight_count(HORSE, 5)
                     + weight_count(JANGGI_ELEPHANT, 3)
                     + weight_count(WAZIR, 3)
                     + weight_count(SOLDIER, 2)
                     - 1;
      result = materialCount > 0 ? VALUE_COUNT_WIN : -VALUE_COUNT_WIN;
      break;
  case UNWEIGHTED_MATERIAL:
      if (var->materialCountingPieceTypes == NO_PIECE_SET || (var->materialCountingPieceTypes & ALL_PIECES))
          result =  count(WHITE, ALL_PIECES) > count(BLACK, ALL_PIECES) ?  VALUE_COUNT_WIN
                  : count(WHITE, ALL_PIECES) < count(BLACK, ALL_PIECES) ? -VALUE_COUNT_WIN
                                                                        :  VALUE_DRAW;
      else
      {
          int subsetCount = 0;
          for (PieceSet ps = var->materialCountingPieceTypes; ps; )
          {
              PieceType pt = pop_lsb(ps);
              subsetCount += count(WHITE, pt) - count(BLACK, pt);
          }
          result = subsetCount > 0 ? VALUE_COUNT_WIN
                 : subsetCount < 0 ? -VALUE_COUNT_WIN
                                   : VALUE_DRAW;
      }
      break;
  case WHITE_DRAW_ODDS:
      result = VALUE_COUNT_WIN;
      break;
  case BLACK_DRAW_ODDS:
      result = -VALUE_COUNT_WIN;
      break;
  case CONNECT_N_COUNT:
      materialCount = connect_line_count(WHITE) - connect_line_count(BLACK);
      result = materialCount > 0 ? VALUE_COUNT_WIN
             : materialCount < 0 ? -VALUE_COUNT_WIN
                                 : VALUE_DRAW;
      break;
  default:
      assert(false);
      result = VALUE_DRAW;
  }
  return sideToMove == WHITE ? result : -result;
}

inline int Position::connect_line_count(Color c) const {
  if (connect_n() <= 0)
      return 0;

  Bitboard connectPieces = 0;
  for (PieceSet ps = connect_piece_types(); ps;) {
      PieceType pt = pop_lsb(ps);
      connectPieces |= pieces(c, pt);
  }

  if (popcount(connectPieces) < connect_n())
      return 0;

  int countLines = 0;
  if (!var->connectLines.empty() && connect_n() == int(var->connectLines.front().size()))
  {
      for (const auto& line : var->connectLines)
      {
          bool complete = true;
          for (Square s : line)
              complete &= bool(connectPieces & square_bb(s));
          countLines += complete;
      }
      return countLines;
  }

  for (Direction d : var->connectDirections)
  {
      Bitboard b = connectPieces;
      for (int i = 1; i < connect_n() && b; i++)
          b &= shift(d, b);
      countLines += popcount(b);
  }
  return countLines;
}

inline void Position::add_to_hand(Piece pc) {
  if (variant()->freeDrops) return;
  pieceCountInHand[color_of(pc)][type_of(pc)]++;
  pieceCountInHand[color_of(pc)][ALL_PIECES]++;
  priorityDropCountInHand[color_of(pc)] += bool(var->isPriorityDrop & piece_set(type_of(pc)));
  psq += PSQT::psq[pc][SQ_NONE];
}

inline void Position::remove_from_hand(Piece pc) {
  if (variant()->freeDrops) return;
  pieceCountInHand[color_of(pc)][type_of(pc)]--;
  pieceCountInHand[color_of(pc)][ALL_PIECES]--;
  priorityDropCountInHand[color_of(pc)] -= bool(var->isPriorityDrop & piece_set(type_of(pc)));
  psq -= PSQT::psq[pc][SQ_NONE];
}

inline int Position::add_to_prison(Piece pc) {
  if (variant()->captureType != PRISON) return 0;
  Color prison = ~color_of(pc);
  int n = ++pieceCountInPrison[prison][type_of(pc)];
  pieceCountInPrison[prison][ALL_PIECES]++;
  return n;
}

inline int Position::remove_from_prison(Piece pc) {
  if (variant()->captureType != PRISON) return 0;
  Color prison = ~color_of(pc);
  int n = --pieceCountInPrison[prison][type_of(pc)];
  pieceCountInPrison[prison][ALL_PIECES]--;
  return n;
}

inline void Position::drop_piece(Piece pc_hand, Piece pc_drop, Square s, PieceType exchange) {
  assert(can_drop(color_of(pc_hand), type_of(pc_hand)) || var->twoBoards || exchange != NO_PIECE_TYPE);
  put_piece(pc_drop, s, pc_drop != pc_hand, pc_drop != pc_hand ? pc_hand : NO_PIECE);
  if (exchange) {
    Piece ex = make_piece(~sideToMove, exchange);
    add_to_hand(ex);
    remove_from_prison(ex);
    remove_from_prison(pc_drop);
  } else {
    remove_from_hand(pc_hand);
    virtualPieces += (pieceCountInHand[color_of(pc_hand)][type_of(pc_hand)] < 0);
  }
}

inline void Position::undrop_piece(Piece pc_hand, Square s, PieceType exchange) {
  remove_piece(s);
  board[s] = NO_PIECE;
  if (exchange) {
    Piece ex = make_piece(~sideToMove, exchange);
    remove_from_hand(ex);
    add_to_prison(ex);
    add_to_prison(pc_hand);
  } else {
    virtualPieces -= (pieceCountInHand[color_of(pc_hand)][type_of(pc_hand)] < 0);
    add_to_hand(pc_hand);
  }
  assert(can_drop(color_of(pc_hand), type_of(pc_hand)) || var->twoBoards || exchange != NO_PIECE_TYPE);
}

inline bool Position::can_drop(Color c, PieceType pt) const {
  if (variant()->freeDrops)
      return true;

  if (pt == ALL_PIECES)
      return count_in_hand(c, pt) > 0
          || (variant()->borrowOpponentDropsWhenEmpty
              && count_in_hand(c, ALL_PIECES) == 0
              && count_in_hand(~c, ALL_PIECES) > 0);

  Color handColor = drop_hand_color(c, pt);

  if (count_in_hand(handColor, pt) <= 0)
      return false;

  if (variant()->dropKingLast && pt == king_type())
      return count_in_hand(handColor, ALL_PIECES) <= count_in_hand(handColor, pt);

  return true;
}

inline bool Position::has_exchange() const {
  return count_in_prison(WHITE, ALL_PIECES) > 0 && count_in_prison(BLACK, ALL_PIECES) > 0;
}

inline PieceSet Position::rescueFor(PieceType pt) const {
  return var->hostageExchange[pt];
}

//Returns the pieces that are not moved (including newly added pieces during the game, i.e. DROPS) of a side
inline Bitboard Position::not_moved_pieces(Color c) const {
    return this->st->not_moved_pieces[c];
}

//Returns the places of wall squares
inline Bitboard Position::wall_squares() const {
    return this->st->wallSquares;
}

inline void Position::commit_piece(Piece pc, File fl){
    committedGates[color_of(pc)][fl] = type_of(pc);
}

inline PieceType Position::uncommit_piece(Color cl, File fl){
    PieceType committedPieceType = committedGates[cl][fl];
    committedGates[cl][fl] = NO_PIECE_TYPE;
    return committedPieceType;
}

inline PieceType Position::committed_piece_type(Color cl, File fl) const {
    return committedGates[cl][fl];
}

inline bool Position::has_committed_piece(Color cl, File fl) const {
    return committed_piece_type(cl,fl) > NO_PIECE_TYPE;
}

inline PieceType Position::drop_committed_piece(Color cl, File fl){
    if(has_committed_piece(cl, fl)){
        Square dropSquare = make_square(fl, (cl == WHITE)? RANK_1 : max_rank());
        PieceType committedPieceType = committedGates[cl][fl];
        put_piece(make_piece(cl, committedPieceType), dropSquare, false, NO_PIECE);
        uncommit_piece(cl, fl);
        return committedPieceType;
    }
    else return NO_PIECE_TYPE;
}

} // namespace Stockfish

#endif // #ifndef POSITION_H_INCLUDED
