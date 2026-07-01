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

#include <cassert>
#include <cstdlib>
#include <vector>

#include "movegen.h"
#include "position.h"
#include "thread.h"

namespace Stockfish {

#ifdef USE_HEAP_INSTEAD_OF_STACK_FOR_MOVE_LIST
template<GenType T>
MoveList<T>::MoveList(const Position& pos) {
    thread = pos.this_thread();
    if (thread)
        moveList = thread->acquire_buffer();
    else {
        moveListPtr = std::make_unique<ExtMove[]>(MOVEGEN_OVERFLOW_CAPACITY);
        moveList = moveListPtr.get();
    }
    last = generate<T>(pos, moveList);
    assert(last - moveList <= MOVEGEN_OVERFLOW_CAPACITY);
}

template<GenType T>
MoveList<T>::~MoveList() {
    if (thread)
        thread->release_buffer(moveList);
}

// Explicit instantiations
template struct MoveList<CAPTURES>;
template struct MoveList<QUIETS>;
template struct MoveList<QUIET_CHECKS>;
template struct MoveList<EVASIONS>;
template struct MoveList<NON_EVASIONS>;
#endif

namespace {

  template<GenType Type>
  constexpr bool CanEmitPromotions = Type == CAPTURES || Type == QUIETS || Type == EVASIONS || Type == NON_EVASIONS;

  Bitboard useful_freeze_gates(const Position& pos, Color us) {
    Bitboard gates = 0;
    Bitboard enemies = pos.pieces(~us);
    while (enemies)
      gates |= pos.freeze_zone_from_square(pop_lsb(enemies));
    return gates;
  }

  template<MoveType T>
  ExtMove* make_move_and_gating(const Position& pos, ExtMove* moveList, Color us, Square from, Square to, PieceType pt = NO_PIECE_TYPE) {

    Move m = make<T>(from, to, pt);
    bool iguiShot = T == SPECIAL && from != to;
    bool captureIsRifle = pos.rifle_capture(m) && pos.capture(m);
    bool rifleShot = captureIsRifle && (T == NORMAL || T == PROMOTION);
    Square effectiveTo = (rifleShot || iguiShot) ? from : to;
    Square capSq = pos.capture(m) ? pos.capture_square(m) : SQ_NONE;
    Bitboard occupancyAfter = pos.pieces();
    if (from != effectiveTo) occupancyAfter ^= square_bb(from) ^ square_bb(effectiveTo);
    if (capSq != SQ_NONE) occupancyAfter ^= square_bb(capSq);
    if (T == CASTLING)
    {
        Square kto, rto;
        pos.castling_destinations(us, from, to, kto, rto);
        occupancyAfter = (pos.pieces() ^ square_bb(from) ^ square_bb(to)) | kto | rto;
    }

    // Wall placing moves
    //if it's "wall or move", and they chose non-null move, skip even generating wall move
    if (pos.walling(us) && !(pos.wall_or_move() && (from!=to)))
    {
        const bool pureWallMove = T == SPECIAL && from == to && pt == NO_PIECE_TYPE && pos.wall_or_move();
        Bitboard b = pos.wall_target_mask(us, from, effectiveTo, pureWallMove ? SQ_NONE : to, occupancyAfter);

        while (b)
            *moveList++ = make_gating<T>(from, to, pt, pop_lsb(b));
        return moveList;
    }

    if (pos.laser_game())
    {
        Piece pcFrom = pos.piece_on(from);
        PieceType mt = type_of(pcFrom);
        bool isDosChess = pos.variant()->name == "dos-laser-chess";
        
        if (pos.is_oriented(mt))
        {
            PieceType base_pt = pos.variant()->base_piece_type(mt);
            for (int i = 0; i < 4; ++i)
                *moveList++ = make_gating<T>(from, to, PieceType(base_pt + i), to);
        }
        else
        {
            *moveList++ = m;
        }

        if (isDosChess)
        {
            for (Square sq = SQ_A1; sq < SQUARE_NB; ++sq)
            {
                if (!is_ok(sq) || file_of(sq) > pos.max_file() || rank_of(sq) > pos.max_rank())
                    continue;
                if (sq == to)
                    continue;
                Piece pc = pos.piece_on(sq);
                if (pc != NO_PIECE && color_of(pc) == us && pos.is_oriented(type_of(pc)))
                {
                    PieceType base_pt = pos.variant()->base_piece_type(type_of(pc));
                    for (int i = 0; i < 4; ++i)
                        *moveList++ = make_gating<T>(from, to, PieceType(base_pt + i), sq);
                }
            }
        }
        return moveList;
    }

    PieceType forcedGate = NO_PIECE_TYPE;
    Square forcedGateSquare = SQ_NONE;
    if (from != to)
    {
        Piece pcFrom = pos.piece_on(from);
        if (pcFrom != NO_PIECE && color_of(pcFrom) == us)
        {
            forcedGate = pos.forced_gating_type(us, type_of(pcFrom));
            if (forcedGate != NO_PIECE_TYPE)
                forcedGateSquare = from;
        }
    }

    if (forcedGate != NO_PIECE_TYPE)
    {
        // Only generate forced gating if the target square is not occupied after the base move
        // (e.g., to prevent overwriting/disappearing pieces in rifle capture or special moves)
        if (!(occupancyAfter & forcedGateSquare))
            *moveList++ = make_gating<T>(from, to, forcedGate, forcedGateSquare);
    }
    else
        *moveList++ = m;

    // Gating moves
    if (pos.seirawan_gating() && !captureIsRifle)
    {
        for (Square gateSq : {from, to})
        {
            if (gateSq == to && (T != CASTLING || gateSq == from)) continue;
            if (!(pos.gates(us) & gateSq)) continue;
            if (occupancyAfter & gateSq) continue;

            for (PieceSet ps = pos.piece_types(); ps;)
            {
                PieceType pt_gating = pop_lsb(ps);
                if (pos.can_drop(us, pt_gating) && (pos.drop_region(us, pt_gating) & gateSq))
                {
                    if (pos.symmetric_drop_types() & piece_set(pt_gating))
                    {
                        Square gate2 = pos.mirrored_pair_drop_square(gateSq);
                        if (gate2 != gateSq
                            && (pos.drop_region(us, pt_gating) & gate2)
                            && !(occupancyAfter & gate2)
                            && pos.count_in_hand(pos.drop_hand_color(us, pt_gating), pt_gating) >= 2)
                            *moveList++ = make_gating<T>(from, to, pt_gating, gateSq);
                    }
                    else
                        *moveList++ = make_gating<T>(from, to, pt_gating, gateSq);
                }
            }
        }
    }

    return moveList;
  }

  bool has_any_promotion(const Position& pos, Color us, Square to) {
      auto can_emit_promotion_variant = [&](PieceType pt) {
          return pos.promotion_allowed(us, pt, to)
              && !(pos.prison_pawn_promotion() && pos.count_in_prison(~us, pt) == 0);
      };

      for (PieceSet ps = pos.promotion_piece_types(us, to); ps;)
          if (can_emit_promotion_variant(pop_lsb(ps)))
              return true;
      PieceType pt = pos.promoted_piece_type(PAWN);
      if (pt && pos.promotion_allowed(us, pt, to) && !(pos.piece_promotion_on_capture() && pos.empty(to)))
          return true;
      return false;
  }

  template<GenType Type>
  ExtMove* emit_promotion_variants(const Position& pos, ExtMove* moveList, Color us, Square from, Square to) {
      if constexpr (Type != CAPTURES && Type != QUIETS && Type != EVASIONS && Type != NON_EVASIONS)
          return moveList;

      auto can_emit_promotion_variant = [&](PieceType pt) {
          return pos.promotion_allowed(us, pt, to)
              && !(pos.prison_pawn_promotion() && pos.count_in_prison(~us, pt) == 0);
      };

      for (PieceSet promotions = pos.promotion_piece_types(us, to); promotions;)
      {
          PieceType pt = pop_msb(promotions);
          if (can_emit_promotion_variant(pt))
          {
              moveList = make_move_and_gating<PROMOTION>(pos, moveList, us, from, to, pt);
          }
      }
      PieceType pt = pos.promoted_piece_type(type_of(pos.piece_on(from)));
      if (pt && pos.promotion_allowed(us, pt, to) && !(pos.piece_promotion_on_capture() && pos.empty(to)))
      {
          moveList = make_move_and_gating<PIECE_PROMOTION>(pos, moveList, us, from, to, pt);
      }
      return moveList;
  }

  template<Color Us, GenType Type>
  bool can_generate_drop(const Position& pos, PieceType pt) {
      return pos.can_drop(Us, pt)
          || (Type != NON_EVASIONS && pos.two_boards() && pos.virtual_drops() && pos.allow_virtual_drop(Us, pt));
  }

  template<Color Us, GenType Type>
  ExtMove* emit_drop_forms(const Position& pos, ExtMove* moveList, PieceType pt, Bitboard baseTargets, bool restrictToCheckSquares) {
      PieceSet dropForms = pos.drop_piece_types(pt);
      while (dropForms)
      {
          PieceType dropped = pop_lsb(dropForms);
          Bitboard b2 = baseTargets & pos.drop_region(Us, dropped);
          if (restrictToCheckSquares)
              b2 &= pos.check_squares(dropped);
          while (b2)
              *moveList++ = make_drop(pop_lsb(b2), pt, dropped);
      }
      return moveList;
  }






  template<Color Us, GenType Type>
  ExtMove* generate_drops(const Position& pos, ExtMove* moveList, PieceType pt, Bitboard b) {
    [[maybe_unused]] constexpr bool QuietChecks = Type == QUIET_CHECKS;

    if (pos.edge_insert_only() && (pos.edge_insert_types() & piece_set(pt)))
        return moveList;

    // Do not generate virtual drops for perft and at root
    if (can_generate_drop<Us, Type>(pos, pt))
    {
        // Restrict to valid target
        b &= ~pos.pieces() | pos.opening_swap_drop_targets(Us, pt);

        if ((pos.symmetric_drop_types() & piece_set(pt)) && pos.count_in_hand(pos.drop_hand_color(Us, pt), pt) >= 2)
        {
            b &= pos.drop_region(Us, pt);
            while (b)
            {
                Square to = pop_lsb(b);
                Square to2 = pos.mirrored_pair_drop_square(to);
                if (to2 == to || !(pos.drop_region(Us, pt) & to2) || (pos.pieces() & to2))
                    continue;
                if (to > to2)
                    continue;
                Move m = make_drop_pair(to, to2, pt, pt);
                if (QuietChecks && !pos.gives_check(m))
                    continue;
                *moveList++ = m;
            }
            return moveList;
        }

        moveList = emit_drop_forms<Us, Type>(pos, moveList, pt, b, QuietChecks || !pos.can_drop(Us, pt));
    }

    return moveList;
  }

  template<Color Us, GenType Type>
  ExtMove* generate_capture_drops(const Position& pos, ExtMove* moveList, PieceType pt, Bitboard b) {
    [[maybe_unused]] constexpr bool GeneratesQuiets = Type == QUIETS || Type == QUIET_CHECKS;

    if (GeneratesQuiets)
        return moveList;

    if (pos.edge_insert_only() && (pos.edge_insert_types() & piece_set(pt)))
        return moveList;

    if (!(pos.capture_drop_types() & piece_set(pt)))
        return moveList;

    if (!can_generate_drop<Us, Type>(pos, pt))
        return moveList;

    Bitboard capturable = pos.pieces(~Us);
    Bitboard dropTargets = b;
    if (pos.self_capture(pt))
    {
        Bitboard friendlyCapturable = pos.pieces(Us) & ~pos.pieces(Us, KING);
        capturable |= friendlyCapturable;
        dropTargets |= friendlyCapturable;
    }
    b = dropTargets & capturable;

    moveList = emit_drop_forms<Us, Type>(pos, moveList, pt, b, false);

    return moveList;
  }

  template<Color Us, GenType Type>
  ExtMove* generate_edge_insertions(const Position& pos, ExtMove* moveList) {
    if (Type == QUIET_CHECKS)
        return moveList;

    PieceSet insertTypes = pos.edge_insert_types();
    if (!insertTypes)
        return moveList;

    Bitboard entries = pos.edge_insert_region(Us) & pos.board_bb();
    if (!entries)
        return moveList;

    if (insertTypes & ALL_PIECES)
        insertTypes = pos.piece_types();

    while (entries)
    {
        Square to = pop_lsb(entries);
        auto emit_insert = [&](Square from, PieceType insertPt) {
            Move m = make_insert(from, to, insertPt, insertPt);
            PushInfo pushInfo;
            bool push = pos.analyze_push(m, pushInfo);
            if (!pos.empty(to) && !push)
                return;
            bool cap = push && pushInfo.captures;
            if ((Type == CAPTURES && cap)
                || (Type == QUIETS && !cap)
                || (Type == EVASIONS || Type == NON_EVASIONS))
                *moveList++ = m;
        };
        for (PieceSet ps = insertTypes; ps; )
        {
            PieceType pt = pop_lsb(ps);
            if (!can_generate_drop<Us, Type>(pos, pt)
                || !(pos.drop_region(Us, pt) & to))
                continue;

            if (pos.edge_insert_direction_ok(Us, to + SOUTH, to))
                emit_insert(to + SOUTH, pt);
            if (pos.edge_insert_direction_ok(Us, to + NORTH, to))
                emit_insert(to + NORTH, pt);
            if (pos.edge_insert_direction_ok(Us, to + EAST, to))
                emit_insert(to + EAST, pt);
            if (pos.edge_insert_direction_ok(Us, to + WEST, to))
                emit_insert(to + WEST, pt);
        }
    }

    return moveList;
  }

  template<Color Us, GenType Type>
  ExtMove* generate_exchanges(const Position& pos, ExtMove* moveList, PieceType pt, Bitboard b) {
      assert(Type != CAPTURES);
      static_assert(SQUARE_BITS >= PIECE_TYPE_BITS, "not enough bits for exchange move");
      Color opp = ~Us;
      if (pos.count_in_prison(opp, pt) > 0) {
          PieceSet rescue = NO_PIECE_SET;
          for (PieceSet r = pos.rescueFor(pt); r; ) {
              PieceType ex = pop_lsb(r);
              if (pos.count_in_prison(Us, ex) > 0) {
                  rescue |= ex;
              }
          }
          if (rescue == NO_PIECE_SET) {
              return moveList;
          }
          auto emit_exchanges = [&](Bitboard targets, PieceType finalPt) {
              targets &= pos.drop_region(Us, finalPt);
              while (targets) {
                  auto to = pop_lsb(targets);
                  for (PieceSet r = rescue; r; ) {
                      PieceType ex = pop_lsb(r);
                      *moveList++ = make_exchange(to, ex, pt, finalPt);
                  }
              }
          };

          if (pos.drop_promoted() && pos.promoted_piece_type(pt)) {
              Bitboard promotedTargets = b;
              if (Type == QUIET_CHECKS)
                  promotedTargets &= pos.check_squares(pos.promoted_piece_type(pt));
              emit_exchanges(promotedTargets, pos.promoted_piece_type(pt));
          }
          if (Type == QUIET_CHECKS)
              b &= pos.check_squares(pt);
          emit_exchanges(b, pt);
      }
      return moveList;
  }

  struct MoveBuffer {
      ExtMove* begin;
      ExtMove* end;
  };

  struct PawnGenSpec {
      Bitboard target;
      Bitboard fromMask = AllSquares;
  };

  struct PieceGenSpec {
      PieceType pt;
      Bitboard target;
      Bitboard captureTarget;
      Bitboard fromMask = AllSquares;
  };

  template<Color Us, GenType Type>
  ExtMove* generate_pawn_moves(const Position& pos, ExtMove* moveList, PawnGenSpec spec) {
    [[maybe_unused]] constexpr bool GeneratesCaptures = Type == CAPTURES || Type == EVASIONS || Type == NON_EVASIONS;
    [[maybe_unused]] constexpr bool GeneratesQuiets = Type != CAPTURES;
    [[maybe_unused]] constexpr bool QuietChecks = Type == QUIET_CHECKS;

    Bitboard target = spec.target;
    Bitboard fromMask = spec.fromMask;

    constexpr Color     Them     = ~Us;
    constexpr Direction Up       = pawn_push(Us);
    constexpr Direction Up2      = Direction(2 * int(Up));
    constexpr Direction Up3      = Direction(3 * int(Up));
    constexpr Direction UpRight  = (Us == WHITE ? NORTH_EAST : SOUTH_WEST);
    constexpr Direction UpLeft   = (Us == WHITE ? NORTH_WEST : SOUTH_EAST);

    const Bitboard promotionZone = pos.promotion_zone(Us, PAWN);
    const Bitboard standardPromotionZone = pos.sittuyin_promotion() ? Bitboard(0) : promotionZone;
    const Bitboard doubleStepRegion = pos.double_step_region(Us, PAWN);
    const Bitboard tripleStepRegion = pos.triple_step_region(Us, PAWN);

    const Bitboard frozen     = pos.freeze_squares();
    const Bitboard pawns      = pos.pieces(Us, PAWN) & fromMask & ~frozen;
    if (!pawns)
        return moveList;

    Bitboard localCaptureTarget = target;
    if (pos.self_capture(PAWN) && GeneratesCaptures)
        localCaptureTarget |= pos.pieces(Us) & ~pos.pieces(Us, KING) & (Type == EVASIONS ? target : AllSquares);

    const Bitboard unmovedPawns = pawns;
    const Bitboard neutral    = pos.dead_squares();
    Bitboard occupancy = pos.pieces();
    if (const SpellContext* spellCtx = current_spell_context(); spellCtx && Us == pos.side_to_move())
        occupancy &= ~spellCtx->jumpRemoved;
    const Bitboard movable    = pos.board_bb(Us, PAWN) & ~occupancy;
    const Bitboard friendlyCapturable = pos.pieces(Us) & ~pos.pieces(Us, KING);
    const Bitboard capturable = pos.board_bb(Us, PAWN)
                              & (pos.self_capture(PAWN) ? (pos.pieces(Them) | friendlyCapturable | neutral)
                                                    :  (pos.pieces(Them) | neutral));

    if (pos.topology_wraps())
    {
        Bitboard mandatoryPromotionZone = pos.mandatory_promotion_zone(Us, PAWN);
        if (pos.mandatory_pawn_promotion())
            mandatoryPromotionZone |= standardPromotionZone;

        Bitboard remaining = pawns;
        while (remaining)
        {
            Square from = pop_lsb(remaining);
            Bitboard quiets = pos.moves_from(Us, PAWN, from) & target;
            Bitboard attacks = pos.attacks_from(Us, PAWN, from) & capturable & localCaptureTarget;
            Bitboard epSquares = pos.attacks_from(Us, PAWN, from) & pos.ep_squares() & ~pos.pieces() & localCaptureTarget;
            Bitboard quietPromotions = quiets & standardPromotionZone;
            Bitboard capturePromotions = attacks & standardPromotionZone;

            if (mandatoryPromotionZone)
            {
                Bitboard blocked = 0;
                for (Bitboard candidates = (quiets | attacks) & mandatoryPromotionZone; candidates; )
                {
                    Square to = pop_lsb(candidates);
                    if (!has_any_promotion(pos, Us, to))
                        blocked |= to;
                }
                quiets &= ~blocked;
                attacks &= ~blocked;
                quietPromotions &= ~blocked;
                capturePromotions &= ~blocked;
                quiets &= ~mandatoryPromotionZone;
                attacks &= ~mandatoryPromotionZone;
            }

            if (QuietChecks)
                quiets &= pos.check_squares(PAWN);

            if (GeneratesQuiets)
                while (quiets)
                    moveList = make_move_and_gating<NORMAL>(pos, moveList, Us, from, pop_lsb(quiets));

            if (GeneratesCaptures)
            {
                while (attacks)
                    moveList = make_move_and_gating<NORMAL>(pos, moveList, Us, from, pop_lsb(attacks));
                while (epSquares)
                {
                    Square epSquare = pop_lsb(epSquares);
                    if (Type == EVASIONS && (target & (epSquare + Up)) && !pos.non_sliding_riders())
                        continue;
                    moveList = make_move_and_gating<EN_PASSANT>(pos, moveList, Us, from, epSquare);
                }
            }

            if constexpr (CanEmitPromotions<Type>)
            {
                while (quietPromotions)
                    moveList = emit_promotion_variants<Type>(pos, moveList, Us, from, pop_lsb(quietPromotions));
                if (GeneratesCaptures)
                    while (capturePromotions)
                        moveList = emit_promotion_variants<Type>(pos, moveList, Us, from, pop_lsb(capturePromotions));
            }
        }

        return moveList;
    }

    Bitboard b1 = shift<Up>(pawns) & movable & target;
    Bitboard b2 = shift<Up>(shift<Up>(unmovedPawns & doubleStepRegion) & movable) & movable & target;
    Bitboard b3 = shift<Up>(shift<Up>(shift<Up>(unmovedPawns & tripleStepRegion) & movable) & movable) & movable & target;
    Bitboard brc = shift<UpRight>(pawns) & capturable & localCaptureTarget;
    Bitboard blc = shift<UpLeft >(pawns) & capturable & localCaptureTarget;

    Bitboard b1p = b1 & standardPromotionZone;
    Bitboard b2p = b2 & standardPromotionZone;
    Bitboard b3p = b3 & standardPromotionZone;
    Bitboard brcp = brc & standardPromotionZone;
    Bitboard blcp = blc & standardPromotionZone;
    const bool pawnRifleCapture = pos.rifle_capture(make_piece(Us, PAWN));
    Bitboard rifleBrcp = pawnRifleCapture ? brcp : Bitboard(0);
    Bitboard rifleBlcp = pawnRifleCapture ? blcp : Bitboard(0);
    if (pawnRifleCapture)
    {
        brcp = 0;
        blcp = 0;
    }

    Bitboard mandatoryPromotionZone = pos.mandatory_promotion_zone(Us, PAWN);
    if (pos.mandatory_pawn_promotion()) {
        mandatoryPromotionZone |= standardPromotionZone;
    }

    auto emit_normal_moves = [&](Bitboard bb, Direction delta) {
        while (bb)
        {
            Square to = pop_lsb(bb);
            moveList = make_move_and_gating<NORMAL>(pos, moveList, Us, to - delta, to);
        }
    };

    auto filter_promotion_targets = [&](Bitboard bb) {
        Bitboard filtered = 0;
        while (bb)
        {
            Square to = pop_lsb(bb);
            if (has_any_promotion(pos, Us, to))
                filtered |= to;
        }
        return filtered;
    };

    if (mandatoryPromotionZone)
    {
        b1 &= ~mandatoryPromotionZone;
        b2 &= ~mandatoryPromotionZone;
        b3 &= ~mandatoryPromotionZone;
        brc &= ~mandatoryPromotionZone;
        blc &= ~mandatoryPromotionZone;
        b1p = filter_promotion_targets(b1p);
        b2p = filter_promotion_targets(b2p);
        b3p = filter_promotion_targets(b3p);
        brcp = filter_promotion_targets(brcp);
        blcp = filter_promotion_targets(blcp);
    }

    Square ksq = pos.royal_square(Them);
    if (QuietChecks && ksq != SQ_NONE)
    {
        // To make a quiet check, you either make a direct check by pushing a pawn
        // or push a blocker pawn that is not on the same file as the enemy king.
        // Discovered check promotion has been already generated amongst the captures.
        Bitboard dcCandidatePawns = pos.blockers_for_king(Them) & ~file_bb(ksq);
        b1 &= pawn_attacks_bb(Them, ksq) | shift<   Up>(dcCandidatePawns);
        b2 &= pawn_attacks_bb(Them, ksq) | shift<Up2>(dcCandidatePawns);
        b3 &= pawn_attacks_bb(Them, ksq) | shift<Up3>(dcCandidatePawns);
    }

    // Single and double pawn pushes, no promotions
    if (GeneratesQuiets)
    {
        emit_normal_moves(b1, Up);
        emit_normal_moves(b2, Up2);
        emit_normal_moves(b3, Up3);
    }

    // Promotions and underpromotions
    if constexpr (CanEmitPromotions<Type>)
    {
        if (GeneratesCaptures)
        {
            while (brcp)
            {
                Square to = pop_lsb(brcp);
                moveList = emit_promotion_variants<Type>(pos, moveList, Us, to - UpRight, to);
            }

            while (blcp)
            {
                Square to = pop_lsb(blcp);
                moveList = emit_promotion_variants<Type>(pos, moveList, Us, to - UpLeft, to);
            }
        }

        while (b1p)
        {
            Square to = pop_lsb(b1p);
            moveList = emit_promotion_variants<Type>(pos, moveList, Us, to - Up, to);
        }

        while (b2p)
        {
            Square to = pop_lsb(b2p);
            moveList = emit_promotion_variants<Type>(pos, moveList, Us, to - Up2, to);
        }

        while (b3p)
        {
            Square to = pop_lsb(b3p);
            moveList = emit_promotion_variants<Type>(pos, moveList, Us, to - Up3, to);
        }
    }

    if (GeneratesCaptures)
    {
        emit_normal_moves(rifleBrcp, UpRight);
        emit_normal_moves(rifleBlcp, UpLeft);
    }

    // Sittuyin promotions
    if (pos.sittuyin_promotion() && GeneratesCaptures)
    {
        // Pawns need to be in promotion zone if there is more than one pawn
        Bitboard promotionPawns = pos.count<PAWN>(Us) > 1 ? pawns & promotionZone : pawns;
        while (promotionPawns)
        {
            Square from = pop_lsb(promotionPawns);
            for (PieceSet ps = pos.promotion_piece_types(Us); ps;)
            {
                PieceType pt = pop_msb(ps);
                if (!pos.promotion_allowed(Us, pt))
                    continue;
                Bitboard b = (pos.attacks_from(Us, pt, from) & ~(pos.pieces() | pos.dead_squares())) & target;
                if (Type == EVASIONS ? bool(target & from) : true)
                    b |= from;
                while (b)
                {
                    Square to = pop_lsb(b);
                    if (!(pos.attacks_bb(Us, pt, to, pos.pieces() ^ from) & pos.pieces(Them)))
                        *moveList++ = make<PROMOTION>(from, to, pt);
                }
            }
        }
    }

    // Standard and en passant captures
    if (GeneratesCaptures)
    {
        emit_normal_moves(brc, UpRight);
        emit_normal_moves(blc, UpLeft);

        for (Bitboard epSquares = pos.ep_squares() & ~(pos.pieces() | pos.dead_squares()); epSquares; )
        {
            Square epSquare = pop_lsb(epSquares);

            // An en passant capture cannot resolve a discovered check (unless there non-sliding riders)
            if (Type == EVASIONS && (target & (epSquare + Up)) && !pos.non_sliding_riders())
                continue;

            Bitboard b = pawns & pawn_attacks_bb(Them, epSquare);

            // En passant square is already disabled for non-fairy variants if there is no attacker
            assert(b || !pos.fast_attacks());

            while (b)
                moveList = make_move_and_gating<EN_PASSANT>(pos, moveList, Us, pop_lsb(b), epSquare);
        }
    }

    return moveList;
  }


  template<Color Us, GenType Type>
  ExtMove* generate_moves(const Position& pos, ExtMove* moveList, PieceGenSpec spec) {
    [[maybe_unused]] constexpr bool GeneratesCaptures = Type == CAPTURES || Type == EVASIONS || Type == NON_EVASIONS;
    [[maybe_unused]] constexpr bool GeneratesQuiets = Type != CAPTURES;
    [[maybe_unused]] constexpr bool QuietChecks = Type == QUIET_CHECKS;

    const PieceType Pt = spec.pt;
    Bitboard target = spec.target;
    Bitboard captureTarget = spec.captureTarget;
    Bitboard fromMask = spec.fromMask;

    assert(Pt != KING);

    constexpr Direction Up = pawn_push(Us);
    Bitboard bb = pos.pieces(Us, Pt) & fromMask & ~pos.freeze_squares();
    Bitboard selfCaptureTargets = pos.pieces(Us);
    Square royalSq = pos.royal_square(Us);
    if (royalSq != SQ_NONE)
        selfCaptureTargets &= ~square_bb(royalSq);

    while (bb)
    {
        Square from = pop_lsb(bb);

        Bitboard attacks = pos.attacks_from(Us, Pt, from);
        Bitboard quiets = pos.moves_from(Us, Pt, from);
        Bitboard captureSquares;
        Bitboard localCaptureTarget = captureTarget;
        if (pos.self_capture(Pt) && GeneratesCaptures)
            localCaptureTarget |= selfCaptureTargets & (Type == EVASIONS ? target : AllSquares);
        if (pos.anti_royal_self_capture_only() && (pos.anti_royal_types() & piece_set(Pt)))
            captureSquares = attacks & selfCaptureTargets;
        else
        {
            Bitboard capturable = (pos.pieces() & ~pos.pieces(Us)) | pos.dead_squares();
            if (pos.self_capture(Pt) && GeneratesCaptures)
                capturable |= selfCaptureTargets;
            captureSquares = (attacks & capturable) & localCaptureTarget;
        }
        if (Type == QUIETS || Type == QUIET_CHECKS)
            captureSquares = 0;
        Bitboard quietSquares   = (quiets & ~pos.pieces()) & target;
        Bitboard b = captureSquares | quietSquares;
        Bitboard epSquares = (pos.en_passant_types(Us) & piece_set(Pt)) ? (attacks & pos.ep_squares() & ~pos.pieces()) : Bitboard(0);
        Bitboard b1 = b & ~epSquares;
        Bitboard pawnLikeDoubleSteps = 0;
        Bitboard pawnLikeTripleSteps = 0;
        Bitboard promotion_zone = pos.promotion_zone(Us, Pt);
        Bitboard mandatoryPromotionZone = pos.mandatory_promotion_zone(Us, Pt);
        PieceType promPt = pos.is_promoted(from) ? NO_PIECE_TYPE : pos.promoted_piece_type(Pt);
        Bitboard b2 = promPt ? b1 : Bitboard(0);
        Bitboard b3 = pos.piece_demotion() && pos.is_promoted(from) ? b1 : Bitboard(0);
        Bitboard pawnPromotions = (pos.promotion_pawn_types(Us) & piece_set(Pt))
                                ? (b & (Type == EVASIONS ? target : (~pos.pieces(Us) | (pos.self_capture(Pt) ? selfCaptureTargets : Bitboard(0)))) & promotion_zone)
                                : Bitboard(0);
        Bitboard jumpCaptures = 0;
        PieceType movePt = Pt;
        const PieceInfo* pi = pieceMap.get(movePt);
        if (pi->has_universal_hopper())
        {
            // Universal hopper jump captures land on empty squares; captured hurdle
            // square is resolved by jump_capture_square().
            Bitboard candidates = (attacks | quiets) & ~pos.pieces();
            candidates |= pos.universal_hopper_potential_bb(movePt, from) & pos.board_bb() & ~pos.pieces();

            while (candidates)
            {
                Square to = pop_lsb(candidates);
                Square hurdle = pos.jump_capture_square(from, to);
                if (hurdle != SQ_NONE)
                {
                    bool ok = true;
                    if constexpr (Type == EVASIONS)
                    {
                        Bitboard checkers = pos.evasion_checkers();
                        if (checkers & hurdle)
                        {
                            Bitboard remaining = checkers & ~square_bb(hurdle);
                            while (remaining)
                            {
                                Square checksq = pop_lsb(remaining);
                                if (!(pos.checker_evasion_targets(Us, royalSq, checksq) & to))
                                {
                                    ok = false;
                                    break;
                                }
                            }
                        }
                        else
                        {
                            ok = (target & to);
                        }
                    }
                    if (ok)
                        jumpCaptures |= to;
                }
            }
        }
        Bitboard pushMoves = 0;
        if (pos.pushing_strength(Pt) > 0)
        {
            Bitboard candidates = pos.push_targets_from(Us, Pt, from);
            while (candidates)
            {
                Square to = pop_lsb(candidates);
                Move pm = make<NORMAL>(from, to);
                PushInfo pushInfo;
                if (!pos.analyze_push(pm, pushInfo))
                    continue;
                if (pushInfo.captures)
                {
                    if (GeneratesCaptures)
                        pushMoves |= to;
                }
                else if (GeneratesQuiets && (!QuietChecks || pos.gives_check(pm)))
                    pushMoves |= to;
            }
        }

        if (mandatoryPromotionZone)
        {
            b1 &= ~mandatoryPromotionZone;
            jumpCaptures &= ~mandatoryPromotionZone;
        }

        // target squares considering pawn promotions
        if (pawnPromotions && pos.mandatory_pawn_promotion())
        {
            b1 &= ~pawnPromotions;
            jumpCaptures &= ~pawnPromotions;
        }

        // Restrict target squares considering promotion zone
        if (b2 | b3)
        {
            if (pos.mandatory_piece_promotion())
                b1 &= (promotion_zone & from ? Bitboard(0) : ~promotion_zone) | (pos.piece_promotion_on_capture() ? ~pos.pieces() : Bitboard(0));
            // Exclude quiet promotions/demotions
            if (pos.piece_promotion_on_capture())
            {
                b2 &= pos.pieces();
                b3 &= pos.pieces();
            }
            // Consider promotions/demotions into promotion zone
            if (!(promotion_zone & from))
            {
                b2 &= promotion_zone;
                b3 &= promotion_zone;
            }
        }

        if (Type == QUIET_CHECKS)
        {
            b1 &= pos.check_squares(Pt);
            if (b2)
                b2 &= pos.check_squares(pos.promoted_piece_type(Pt));
            if (b3)
                b3 &= pos.check_squares(type_of(pos.unpromoted_piece_on(from)));
        }

        const PieceInfo* pieceInfo = pieceMap.get(Pt);
        if (Type != CAPTURES
            && Pt != PAWN
            && (pos.pawn_like_types(Us) & piece_set(Pt))
            && !pieceInfo->has_explicit_initial_moves())
        {
            Square oneAhead = from + Up;
            if (is_ok(oneAhead) && (quiets & oneAhead))
            {
                Square twoAhead = oneAhead + Up;
                if (   (pos.double_step_region(Us, Pt) & from)
                    && is_ok(twoAhead)
                    && !(b1 & twoAhead)
                    && !(pos.pieces() & twoAhead)
                    && (pos.board_bb(Us, Pt) & twoAhead)
                    && (target & twoAhead))
                    pawnLikeDoubleSteps |= twoAhead;

                Square threeAhead = twoAhead + Up;
                if (   (pos.triple_step_region(Us, Pt) & from)
                    && is_ok(twoAhead)
                    && is_ok(threeAhead)
                    && !(b1 & twoAhead)
                    && !(b1 & threeAhead)
                    && !(pawnLikeDoubleSteps & twoAhead)
                    && !(pos.pieces() & (twoAhead | threeAhead))
                    && (pos.board_bb(Us, Pt) & threeAhead)
                    && (target & threeAhead))
                    pawnLikeTripleSteps |= threeAhead;
            }
        }

        // Jump captures are emitted explicitly below in capture-generating modes.
        // Exclude them from regular NORMAL generation to avoid duplicates.
        b1 &= ~jumpCaptures;
        b1 &= ~pushMoves;

        while (b1)
            moveList = make_move_and_gating<NORMAL>(pos, moveList, Us, from, pop_lsb(b1));

        while (pushMoves)
            moveList = make_move_and_gating<NORMAL>(pos, moveList, Us, from, pop_lsb(pushMoves));

        while (pawnLikeDoubleSteps)
            moveList = make_move_and_gating<NORMAL>(pos, moveList, Us, from, pop_lsb(pawnLikeDoubleSteps));

        while (pawnLikeTripleSteps)
            moveList = make_move_and_gating<NORMAL>(pos, moveList, Us, from, pop_lsb(pawnLikeTripleSteps));

        // Shogi-style piece promotions
        while (b2)
            moveList = make_move_and_gating<PIECE_PROMOTION>(pos, moveList, Us, from, pop_lsb(b2));

        // Piece demotions
        while (b3)
            moveList = make_move_and_gating<PIECE_DEMOTION>(pos, moveList, Us, from, pop_lsb(b3));

        // Pawn-style promotions
        if constexpr (CanEmitPromotions<Type>)
        {
            if (pawnPromotions)
                for (Bitboard promotions = pawnPromotions; promotions; )
                {
                    Square to = pop_lsb(promotions);
                    moveList = emit_promotion_variants<Type>(pos, moveList, Us, from, to);
                }
        }

        // En passant captures
        if (GeneratesCaptures)
        {
            while (jumpCaptures)
                moveList = make_move_and_gating<NORMAL>(pos, moveList, Us, from, pop_lsb(jumpCaptures));
            while (epSquares)
                moveList = make_move_and_gating<EN_PASSANT>(pos, moveList, Us, from, pop_lsb(epSquares));
        }
    }

    return moveList;
  }


  template<Color Us, GenType Type>
  ExtMove* generate_all_impl(const Position& pos, ExtMove* moveList) {

    static_assert(Type != LEGAL, "Unsupported type in generate_all()");

    constexpr bool Checks = Type == QUIET_CHECKS; // Reduce template instantiations
    const PieceType royalPt = pos.royal_piece_type(Us);
    const Square royalSq = pos.royal_square(Us);
    const Bitboard checkers = pos.evasion_checkers();
    Bitboard target;
    Bitboard captureTarget = Bitboard(0);
    Bitboard forcedFromMask = AllSquares;
    bool restrictToForcedJumper = false;
    PieceType forcedJumpPt = NO_PIECE_TYPE;

    if (pos.in_opening_self_removal_phase())
    {
        Bitboard removals = pos.opening_self_removal_targets(Us);
        while (removals)
        {
            Square sq = pop_lsb(removals);
            *moveList++ = make<SPECIAL>(sq, sq);
        }
        return moveList;
    }

    Square forcedSquare = pos.forced_jump_square();
    if (forcedSquare != SQ_NONE && pos.has_forced_jump_followup())
    {
        Piece forcedPiece = pos.piece_on(forcedSquare);
        if (forcedPiece != NO_PIECE) {
            if (color_of(forcedPiece) == Us)
            {
                restrictToForcedJumper = true;
                forcedJumpPt = type_of(forcedPiece);
                forcedFromMask = square_bb(forcedSquare);
            }
            else
            {
                // Opponent must pass while the other side completes a forced jump chain.
                Bitboard usPieces = pos.pieces(Us);
                if (Type != QUIET_CHECKS && pos.pass(Us))
                {
                    Square passSq = usPieces ? lsb(usPieces) : lsb(pos.board_bb());
                    *moveList++ = make<SPECIAL>(passSq, passSq);
                }
                return moveList;
            }
        }
    }

    const PieceInfo* pawnInfo = pieceMap.get(PAWN);
    const bool pawnHasCustomNonStepMovement = pawnInfo->has_nonstandard_pawn_movement();
    const bool useFastStandardPawnGenerator =
           !pawnHasCustomNonStepMovement
        && !pawnInfo->has_explicit_initial_moves()
        && pawnInfo->steps[0][MODALITY_QUIET].size() == 1
        && pawnInfo->steps[0][MODALITY_QUIET].count(NORTH)
        && pawnInfo->steps[0][MODALITY_CAPTURE].size() == 2
        && pawnInfo->steps[0][MODALITY_CAPTURE].count(NORTH_EAST)
        && pawnInfo->steps[0][MODALITY_CAPTURE].count(NORTH_WEST)
        && pawnInfo->slider[0][MODALITY_QUIET].empty()
        && pawnInfo->slider[0][MODALITY_CAPTURE].empty()
        && pawnInfo->tupleSteps[0][MODALITY_QUIET].empty()
        && pawnInfo->tupleSteps[0][MODALITY_CAPTURE].empty()
        && pawnInfo->tupleSlider[0][MODALITY_QUIET].empty()
        && pawnInfo->tupleSlider[0][MODALITY_CAPTURE].empty();

    // Skip generating non-king moves when in double check
    if (Type != EVASIONS || !more_than_one(checkers & ~pos.non_sliding_riders()))
    {
        if (restrictToForcedJumper)
        {
            target = Bitboard(0);
            captureTarget = pos.pieces(~Us) | pos.dead_squares();
        }
        else
        {
            target = Type == EVASIONS     ?  between_bb(royalSq, lsb(checkers))
                   : Type == NON_EVASIONS ? ~pos.pieces( Us)
                   : Type == CAPTURES     ? (pos.pieces(~Us) | pos.dead_squares())
                                          : ~pos.pieces(   ); // QUIETS || QUIET_CHECKS

            if (Type == EVASIONS)
            {
                const bool multipleCheckers = more_than_one(checkers);
                if (multipleCheckers)
                {
                    target = AllSquares;
                    Bitboard remaining = checkers;
                    while (remaining)
                        target &= pos.checker_evasion_targets(Us, royalSq, pop_lsb(remaining));
                }
                else
                    target = pos.checker_evasion_targets(Us, royalSq, lsb(checkers));

                if (pos.blast_on_move() || pos.blast_on_self_destruct())
                    target = AllSquares;
            }

            // Remove inaccessible squares (outside board + wall squares)
            target &= pos.board_bb();

            if (Type == EVASIONS && (pos.blast_on_capture() || pos.blast_on_self_destruct()))
                captureTarget = pos.board_bb() & ~pos.pieces(Us);
            else
                captureTarget = target;
        }

            if (restrictToForcedJumper)
            {
                if (forcedJumpPt == PAWN && forcedJumpPt != royalPt)
                    moveList = useFastStandardPawnGenerator
                             ? generate_pawn_moves<Us, Type>(pos, moveList, PawnGenSpec{target, forcedFromMask})
                             : generate_moves<Us, Type>(pos, moveList, PieceGenSpec{PAWN, target, captureTarget, forcedFromMask});
                else if (forcedJumpPt != royalPt)
                    moveList = generate_moves<Us, Type>(pos, moveList, PieceGenSpec{forcedJumpPt, target, captureTarget, forcedFromMask});
            }
            else
            {
                if (royalPt != PAWN)
                    moveList = useFastStandardPawnGenerator
                             ? generate_pawn_moves<Us, Type>(pos, moveList, PawnGenSpec{target, forcedFromMask})
                             : generate_moves<Us, Type>(pos, moveList, PieceGenSpec{PAWN, target, captureTarget, forcedFromMask});

                PieceSet nonRoyalTypes = pos.piece_types() & ~(piece_set(PAWN) | KING);
                if (royalPt != NO_PIECE_TYPE && royalPt != KING)
                    nonRoyalTypes &= ~piece_set(royalPt);
                for (PieceSet ps = nonRoyalTypes; ps;)
                    moveList = generate_moves<Us, Type>(pos, moveList, PieceGenSpec{pop_lsb(ps), target, captureTarget, forcedFromMask});
            }
        const bool canGenerateDrops = !restrictToForcedJumper
                                   && pos.piece_drops()
                                   && (pos.can_drop(Us, ALL_PIECES) || pos.two_boards());
        const bool generateQuietDrops = canGenerateDrops && Type != CAPTURES;

        // generate drops
        if (canGenerateDrops)
        {
            if (generateQuietDrops)
                for (PieceSet ps = pos.piece_types(); ps;)
                    moveList = generate_drops<Us, Type>(pos, moveList, pop_lsb(ps), target);
            for (PieceSet ps = pos.piece_types(); ps;)
                moveList = generate_capture_drops<Us, Type>(pos, moveList, pop_lsb(ps), captureTarget);
            moveList = generate_edge_insertions<Us, Type>(pos, moveList);
        }
        // generate exchange
        if (!restrictToForcedJumper && pos.capture_type() == PRISON && Type != CAPTURES && pos.has_exchange())
            for (PieceSet ps = pos.piece_types(); ps;)
                moveList = generate_exchanges<Us, Type>(pos, moveList, pop_lsb(ps), target & ~pos.pieces(~Us));

        // Castling with non-king piece
        if constexpr (Type != CAPTURES && Type != QUIET_CHECKS)
            if (!restrictToForcedJumper && !pos.count<KING>(Us) && pos.can_castle(Us == WHITE ? WHITE_CASTLING : BLACK_CASTLING))
            {
                Square from = pos.castling_king_square(Us);
                for(CastlingRights cr : { Us & KING_SIDE, Us & QUEEN_SIDE } )
                    if (!pos.castling_impeded(cr) && pos.can_castle(cr))
                        moveList = make_move_and_gating<CASTLING>(pos, moveList, Us, from, pos.castling_rook_square(cr));
            }

        // Special moves
        if constexpr (Type != CAPTURES && Type != QUIET_CHECKS)
        if (!restrictToForcedJumper && pos.cambodian_moves() && pos.gates(Us))
        {
            if constexpr (Type != EVASIONS)
            if (pos.pieces(Us, KING) & pos.gates(Us))
            {
                Square from = pos.square<KING>(Us);
                Bitboard b = PseudoAttacks[WHITE][KNIGHT][from] & rank_bb(rank_of(from + (Us == WHITE ? NORTH : SOUTH)))
                    & target & ~pos.pieces();
                while (b)
                    moveList = make_move_and_gating<SPECIAL>(pos, moveList, Us, from, pop_lsb(b));
            }

            Bitboard b = pos.pieces(Us, FERS) & pos.gates(Us);
            while (b)
            {
                Square from = pop_lsb(b);
                Square to = from + 2 * (Us == WHITE ? NORTH : SOUTH);
                if (is_ok(to) && (target & to & ~pos.pieces()))
                    moveList = make_move_and_gating<SPECIAL>(pos, moveList, Us, from, to);
            }
        }

        if (!restrictToForcedJumper && pos.gates(Us))
        {
            for (PieceSet ps = pos.piece_types(); ps;)
            {
                PieceType pt = pop_lsb(ps);
                PieceType extraPt = pos.first_move_piece_type(pt);
                if (extraPt == NO_PIECE_TYPE)
                    continue;

                Bitboard froms = pos.pieces(Us, pt) & pos.gates(Us);
                while (froms)
                {
                    Square from = pop_lsb(froms);
                    Bitboard b = (pos.moves_from(Us, extraPt, from) | pos.attacks_from(Us, extraPt, from)) & target & ~pos.pieces(Us);
                    if (Type == QUIET_CHECKS)
                        b &= pos.check_squares(extraPt);
                    while (b)
                        *moveList++ = make<SPECIAL>(from, pop_lsb(b), extraPt);
                }
            }
        }

        if (!restrictToForcedJumper && pos.clone_move_types())
        {
            Bitboard cloneTargets = Type == CAPTURES   ? captureTarget & pos.pieces(~Us)
                                 : Type == QUIETS     ? target & ~pos.pieces()
                                 : Type == QUIET_CHECKS ? target & ~pos.pieces()
                                 : target & ~pos.pieces(Us);

            for (PieceSet ps = pos.clone_move_types(); ps;)
            {
                PieceType pt = pop_lsb(ps);
                Bitboard froms = pos.pieces(Us, pt);
                while (froms)
                {
                    Square from = pop_lsb(froms);
                    Bitboard b = pos.clone_targets_from(Us, from) & cloneTargets;
                    if (Type == QUIET_CHECKS)
                        b &= pos.check_squares(pt);
                    while (b)
                        *moveList++ = make<SPECIAL>(from, pop_lsb(b));
                }
            }
        }

        if (!restrictToForcedJumper && pos.has_pulling()
            && Type != CAPTURES)
        {
            for (PieceSet ps = pos.piece_types(); ps;)
            {
                PieceType pt = pop_lsb(ps);
                if (pos.pulling_strength(pt) <= 0)
                    continue;

                Bitboard froms = pos.pieces(Us, pt);
                while (froms)
                {
                    Square from = pop_lsb(froms);
                    Bitboard pullSources = pos.pull_sources_from(Us, from);
                    while (pullSources)
                    {
                        Square pullFrom = pop_lsb(pullSources);
                        Bitboard b = pos.pull_targets_from(Us, from, pullFrom);
                        if (Type == QUIET_CHECKS)
                            b &= target;
                        while (b)
                        {
                            Move m = make_pull(from, pop_lsb(b), pullFrom);
                            if (Type == QUIET_CHECKS && !pos.gives_check(m))
                                continue;
                            *moveList++ = m;
                        }
                    }
                }
            }
        }

        if (!restrictToForcedJumper && pos.has_adjacent_swapping()
            && Type != CAPTURES)
        {
            for (PieceSet ps = pos.adjacent_swap_move_types(); ps;)
            {
                PieceType pt = pop_lsb(ps);
                Bitboard froms = pos.pieces(Us, pt);
                while (froms)
                {
                    Square from = pop_lsb(froms);
                    Bitboard b = pos.adjacent_swap_targets_from(Us, from);
                    if (Type == QUIET_CHECKS)
                        b &= target;
                    while (b)
                    {
                        Move m = make<SWAP>(from, pop_lsb(b));
                        if (Type == QUIET_CHECKS && !pos.gives_check(m))
                            continue;
                        *moveList++ = m;
                    }
                }
            }
        }

        // Workaround for passing: Execute a non-move with any piece
        if (!restrictToForcedJumper && Type != QUIET_CHECKS && pos.pass(Us) && !pos.count<KING>(Us))
        {
            Bitboard usPieces = pos.pieces(Us);
            Square passSq = usPieces ? lsb(usPieces) : lsb(pos.board_bb());
            *moveList++ = make<SPECIAL>(passSq, passSq);
        }

        if (!restrictToForcedJumper && Type != CAPTURES && pos.self_destruct_types())
        {
            Bitboard b = pos.pieces(Us);
            while (b)
            {
                Square sq = pop_lsb(b);
                Piece mover = pos.piece_on(sq);
                assert(mover != NO_PIECE);
                if (pos.self_destruct_types() & piece_set(type_of(mover)))
                    *moveList++ = make<SPECIAL>(sq, sq, type_of(mover));
            }
        }

        //if "wall or move", generate walling action with null move
        if (!restrictToForcedJumper && pos.walling(Us) && pos.wall_or_move())
        {
            Bitboard usPieces = pos.pieces(Us);
            Bitboard wallAnchors = pos.wall_squares();
            Square wallSq = wallAnchors ? lsb(wallAnchors)
                          : usPieces ? lsb(usPieces)
                          : lsb(pos.board_bb());
            moveList = make_move_and_gating<SPECIAL>(pos, moveList, Us, wallSq, wallSq);
        }

    }

    // Royal moves must not be restricted to checker capture/interposition targets.
    if (royalPt != NO_PIECE_TYPE && royalSq != SQ_NONE
        && !(pos.freeze_squares() & royalSq)
        && (!restrictToForcedJumper || (forcedFromMask & royalSq))
        && (!Checks || pos.topology_wraps() || (pos.blockers_for_king(~Us) & royalSq) || royalPt != KING))
    {
        Bitboard b = 0;
        if (restrictToForcedJumper)
        {
            Bitboard candidates = (pos.attacks_from(Us, royalPt, royalSq)
                                 | pos.moves_from(Us, royalPt, royalSq)) & ~pos.pieces();
            while (candidates)
            {
                Square to = pop_lsb(candidates);
                if (pos.jump_capture_square(royalSq, to) != SQ_NONE)
                    b |= to;
            }
        }
        else
        {
            Bitboard kingAttacks = pos.attacks_from(Us, royalPt, royalSq) & pos.pieces();
            Bitboard kingMoves   = pos.moves_from(Us, royalPt, royalSq) & ~pos.pieces();
            Bitboard kingCaptureMask = Type == EVASIONS ? ~pos.pieces(Us) : captureTarget;
            if (Type != QUIETS && Type != QUIET_CHECKS && pos.self_capture(royalPt))
                kingCaptureMask |= pos.pieces(Us) & ~square_bb(royalSq);
            Bitboard kingQuietMask = Type == EVASIONS ? ~pos.pieces(Us) : target;
            b = (kingAttacks & kingCaptureMask) | (kingMoves & kingQuietMask);
        }
        while (b)
        {
            Square to = pop_lsb(b);
            if constexpr (Type == QUIET_CHECKS)
            {
                ExtMove temp[256];
                ExtMove* tempEnd = make_move_and_gating<NORMAL>(pos, temp, Us, royalSq, to);
                for (ExtMove* it = temp; it != tempEnd; ++it)
                    if (pos.gives_check(it->move))
                        *moveList++ = *it;
            }
            else
                moveList = make_move_and_gating<NORMAL>(pos, moveList, Us, royalSq, to);
        }

        // Passing move by royal piece
        if (!restrictToForcedJumper && pos.pass(Us))
            *moveList++ = make<SPECIAL>(royalSq, royalSq);

        if (royalPt == KING && !restrictToForcedJumper
            && (Type == QUIETS || Type == NON_EVASIONS)
            && pos.can_castle(Us == WHITE ? WHITE_CASTLING : BLACK_CASTLING))
            for (CastlingRights cr : { Us & KING_SIDE, Us & QUEEN_SIDE } )
                if (!pos.castling_impeded(cr) && pos.can_castle(cr))
                    moveList = make_move_and_gating<CASTLING>(pos, moveList, Us, royalSq, pos.castling_rook_square(cr));
    }

    return moveList;
  }

  inline bool is_potion_eligible_base(Move base, MoveType& mt, Square& from, Square& to) {
      if (is_gating(base))
          return false;
      mt = type_of(base);
      // PIECE_PROMOTION is Shogi-style piece promotion and is rejected here because
      // potion gating is a Spell Chess mechanic, which only uses standard chess PROMOTION.
      if (mt != NORMAL && mt != CASTLING && mt != PROMOTION)
          return false;
      if (mt == PROMOTION)
      {
          PieceType prom_pt = promotion_type(base);
          if (prom_pt != KNIGHT && prom_pt != BISHOP && prom_pt != ROOK && prom_pt != QUEEN)
              return false;
      }
      from = from_sq(base);
      to = to_sq(base);
      return true;
  }

  template<GenType Type>
  inline bool potion_move_matches(const Position& pos, Move base, Move m) {
      if constexpr (Type == EVASIONS)
      {
          Color us = pos.side_to_move();
          Bitboard occupied = pos.pieces();
          Square from = from_sq(base);
          Square to = to_sq(base);
          if (type_of(base) == CASTLING)
          {
              Square kto, rto;
              pos.castling_destinations(us, from, to, kto, rto);
              occupied = (occupied ^ square_bb(from) ^ square_bb(to)) | square_bb(kto) | square_bb(rto);
          }
          else if (from != to)
              occupied ^= square_bb(from) ^ square_bb(to);

          if (pos.capture(m))
              occupied ^= square_bb(pos.capture_square(m));

          if (pos.gating_move_blocks_occupancy(m) && gating_square(m) != SQ_NONE)
              occupied |= square_bb(gating_square(m));

          Position::SimulatedMoveGuard guard(pos, m);
          if (pos.attackers_to(pos.royal_square(us), occupied, ~us))
              return false;
          return true;
      }

      const bool baseCaptures = pos.capture(base);

      if constexpr (Type == CAPTURES)
      {
          if (baseCaptures)
              return true;
          return is_promotion_move(base) && promotion_type(base) == QUEEN;
      }
      else if constexpr (Type == QUIETS)
      {
          return !baseCaptures ? !is_promotion_move(base) || promotion_type(base) != QUEEN
                               : is_promotion_move(base) && promotion_type(base) != QUEEN;
      }
      else if constexpr (Type == QUIET_CHECKS)
      {
          return !baseCaptures && type_of(base) != CASTLING && !is_promotion_move(base) && pos.gives_check(m);
      }
      else
      {
          return true;
      }
  }

  enum class AppendStatus { Appended, Skipped, Full };

  struct PotionBaseInfo {
      MoveType mt = NORMAL;
      Square from = SQ_NONE;
      Square to = SQ_NONE;
      Piece mover = NO_PIECE;
      PieceType moverType = NO_PIECE_TYPE;
      bool isInitial = false;
      MoveModality modality = MODALITY_QUIET;
  };

  inline bool prepare_potion_base(const Position& pos, Move base, PotionBaseInfo& info) {
      if (!is_potion_eligible_base(base, info.mt, info.from, info.to))
          return false;

      info.mover = pos.piece_on(info.from);
      if (info.mover == NO_PIECE)
          return false;

      info.moverType = type_of(info.mover);
      info.isInitial = pos.not_moved_pieces(pos.side_to_move()) & info.from;
      info.modality = pos.capture(base) ? MODALITY_CAPTURE : MODALITY_QUIET;
      return true;
  }

  template<GenType Type>
  inline AppendStatus try_append_potion_gating_move(const Position& pos, ExtMove*& cur, ExtMove* maxEnd,
                                                    Square from, Square to, MoveType mt, Move base,
                                                    Variant::PotionType potion, PieceType potionPiece, Square gate, int value) {
      if (cur >= maxEnd)
          return AppendStatus::Full;

      // is_potion_eligible_base() restricts mt to the cases below.
      assert(mt == NORMAL || mt == CASTLING || mt == PROMOTION);
      Move gatingMove = MOVE_NONE;
      switch (mt)
      {
          case NORMAL:
              gatingMove = make_gating<NORMAL>(from, to, potionPiece, gate);
              break;
          case CASTLING:
              gatingMove = make_gating<CASTLING>(from, to, potionPiece, gate);
              break;
          case PROMOTION:
              gatingMove = make_promotion_potion(from, to, promotion_type(base), potion, gate);
              break;
          default:
              break;
      }
      assert(gatingMove != MOVE_NONE);

      if (!potion_move_matches<Type>(pos, base, gatingMove))
          return AppendStatus::Skipped;

      cur->move = gatingMove;
      cur->value = value;
      ++cur;
      return AppendStatus::Appended;
  }

  template<Color Us, GenType Type>
  ExtMove* generate_potion_moves(const Position& pos, MoveBuffer buffer) {
    const Variant* var = pos.variant();
    ExtMove* cur = buffer.end;
    ExtMove* maxEnd = buffer.begin + MOVEGEN_OVERFLOW_CAPACITY;

    for (int pt = 0; pt < Variant::POTION_TYPE_NB; ++pt)
    {
        auto potion = static_cast<Variant::PotionType>(pt);
        PieceType potionPiece = pos.potion_piece(potion);
        if (potionPiece == NO_PIECE_TYPE)
            continue;
        if (!pos.can_cast_potion(Us, potion))
            continue;

        Bitboard candidates = pos.board_bb();
        if (potion == Variant::POTION_JUMP)
            candidates &= pos.pieces();
        else if (!var->potionDropOnOccupied)
            candidates &= ~pos.pieces();

        if (potion == Variant::POTION_FREEZE)
            candidates &= useful_freeze_gates(pos, Us);

        if (potion == Variant::POTION_FREEZE)
        {
            struct PreparedPotionBase {
                Move move;
                int value;
                PotionBaseInfo info;
            };
            std::vector<PreparedPotionBase> bases;
            bases.reserve(buffer.end - buffer.begin);
            for (ExtMove* it = buffer.begin; it != buffer.end; ++it)
            {
                PotionBaseInfo baseInfo;
                if (prepare_potion_base(pos, it->move, baseInfo))
                    bases.push_back({it->move, it->value, baseInfo});
            }

            while (candidates)
            {
                if (cur >= maxEnd)
                    return maxEnd;

                Square gate = pop_lsb(candidates);
                Bitboard newFreezeZone = pos.freeze_zone_from_square(gate);
                for (const auto& base : bases)
                {
                    if (gate == base.info.to)
                        continue;
                    if (pos.freeze_squares() & base.info.from)
                        continue;
                    if (newFreezeZone & base.info.from)
                        continue;
                    if ((between_bb(base.info.from, base.info.to, base.info.moverType, base.info.modality, base.info.isInitial) & ~square_bb(base.info.to)) & gate)
                        continue;

                    if (try_append_potion_gating_move<Type>(pos, cur, maxEnd, base.info.from, base.info.to, base.info.mt, base.move, potion, potionPiece, gate, base.value)
                        == AppendStatus::Full)
                        return maxEnd;
                }
            }
            continue;
        }

        if (potion == Variant::POTION_JUMP)
        {
            if (!candidates)
                continue;

            ScopedSpellContext guard(Bitboard(0), candidates);

#ifdef USE_HEAP_INSTEAD_OF_STACK_FOR_MOVE_LIST
            auto jumpMoves = std::make_unique<ExtMove[]>(MOVEGEN_OVERFLOW_CAPACITY);
            ExtMove* jumpEnd = generate_all_impl<Us, NON_EVASIONS>(pos, jumpMoves.get());
            assert(jumpEnd - jumpMoves.get() <= MOVEGEN_OVERFLOW_CAPACITY);

            for (ExtMove* it = jumpMoves.get(); it != jumpEnd; ++it)
#else
            ExtMove jumpMoves[MOVEGEN_OVERFLOW_CAPACITY];
            ExtMove* jumpEnd = generate_all_impl<Us, NON_EVASIONS>(pos, jumpMoves);
            assert(jumpEnd - jumpMoves <= MOVEGEN_OVERFLOW_CAPACITY);

            for (ExtMove* it = jumpMoves; it != jumpEnd; ++it)
#endif
            {
                if (cur >= maxEnd)
                    return maxEnd;

                PotionBaseInfo baseInfo;
                if (!prepare_potion_base(pos, it->move, baseInfo))
                    continue;

                // Pure leapers cannot have an intermediate path square.
                const bool initial = baseInfo.isInitial && pieceMap.get(baseInfo.moverType)->has_explicit_initial_moves();
                const bool isRider = baseInfo.modality == MODALITY_CAPTURE
                    ? AttackRiderTypes[baseInfo.moverType] != NO_RIDER
                    : MoveRiderTypes[initial][baseInfo.moverType] != NO_RIDER;

                if ((baseInfo.mt == NORMAL || baseInfo.mt == PROMOTION)
                    && !isRider
                    && baseInfo.moverType != PAWN
                    && baseInfo.moverType != SHOGI_PAWN
                    && baseInfo.moverType != SOLDIER)
                    continue;

                if (distance(baseInfo.from, baseInfo.to) <= 1)
                    continue;

                Bitboard path = between_bb(baseInfo.from, baseInfo.to, baseInfo.moverType, baseInfo.modality, baseInfo.isInitial);
                Bitboard intersection = path & candidates & ~square_bb(baseInfo.to);
                if (popcount(intersection) != 1)
                    continue;

                Square gate = lsb(intersection);

                bool moveOk = false;
                {
                    ScopedSpellContext revalGuard(Bitboard(0), square_bb(gate));
                    Bitboard okSquares = baseInfo.isInitial ? (pos.moves_from<true>(Us, baseInfo.moverType, baseInfo.from) | pos.attacks_from<true>(Us, baseInfo.moverType, baseInfo.from))
                                                            : (pos.moves_from<false>(Us, baseInfo.moverType, baseInfo.from) | pos.attacks_from<false>(Us, baseInfo.moverType, baseInfo.from));
                    moveOk = bool(okSquares & baseInfo.to);
                }
                if (!moveOk)
                    continue;

                if (try_append_potion_gating_move<Type>(pos, cur, maxEnd, baseInfo.from, baseInfo.to, baseInfo.mt, it->move, potion, potionPiece, gate, it->value)
                    == AppendStatus::Full)
                    return maxEnd;
            }
        }
    }

    return cur;
  }

  template<Color Us, GenType Type>
  ExtMove* generate_all(const Position& pos, ExtMove* moveList) {
    ExtMove* baseEnd = generate_all_impl<Us, Type>(pos, moveList);
    if (pos.laser_game() && (Type == NON_EVASIONS || Type == QUIETS))
    {
        for (Square sq = SQ_A1; sq < SQUARE_NB; ++sq)
        {
            if (!is_ok(sq) || file_of(sq) > pos.max_file() || rank_of(sq) > pos.max_rank())
                continue;
            Piece pc = pos.piece_on(sq);
            if (pc != NO_PIECE && color_of(pc) == Us && pos.is_oriented(type_of(pc)))
            {
                PieceType base_pt = pos.variant()->base_piece_type(type_of(pc));
                int current_orient = type_of(pc) - base_pt;
                for (int i = 0; i < 4; ++i)
                {
                    if (i == current_orient)
                        continue;
                    *baseEnd++ = make_gating<NORMAL>(sq, sq, PieceType(base_pt + i), sq);
                }
            }
        }
    }
    if (!pos.potions_enabled())
        return baseEnd;
    return generate_potion_moves<Us, Type>(pos, MoveBuffer{moveList, baseEnd});
  }

} // namespace


/// <CAPTURES>     Generates all pseudo-legal captures plus queen promotions
/// <QUIETS>       Generates all pseudo-legal non-captures and underpromotions
/// <EVASIONS>     Generates all pseudo-legal check evasions when the side to move is in check
/// <QUIET_CHECKS> Generates all pseudo-legal non-captures giving check, except castling and promotions
/// <NON_EVASIONS> Generates all pseudo-legal captures and non-captures
///
/// Returns a pointer to the end of the move list.

template<GenType Type>
ExtMove* generate(const Position& pos, ExtMove* moveList) {

  static_assert(Type != LEGAL, "Unsupported type in generate()");
  assert((Type == EVASIONS) == (bool)pos.evasion_checkers()
         || (pos.topology_wraps() && Type == NON_EVASIONS && pos.evasion_checkers()));
  Color us = pos.side_to_move();

  return us == WHITE ? generate_all<WHITE, Type>(pos, moveList)
                     : generate_all<BLACK, Type>(pos, moveList);
}

template<GenType Type>
ExtMove* generate_without_potions(const Position& pos, ExtMove* moveList) {

  static_assert(Type != LEGAL, "Unsupported type in generate_without_potions()");
  assert((Type == EVASIONS) == (bool)pos.evasion_checkers()
         || (pos.topology_wraps() && Type == NON_EVASIONS && pos.evasion_checkers()));
  Color us = pos.side_to_move();
  return us == WHITE ? generate_all_impl<WHITE, Type>(pos, moveList)
                     : generate_all_impl<BLACK, Type>(pos, moveList);
}

template<GenType Type>
ExtMove* append_potions(const Position& pos, ExtMove* listBegin, ExtMove* baseEnd) {

  static_assert(Type != LEGAL, "Unsupported type in append_potions()");
  if (!pos.potions_enabled())
      return baseEnd;
  assert((Type == EVASIONS) == (bool)pos.evasion_checkers()
         || (pos.topology_wraps() && Type == NON_EVASIONS && pos.evasion_checkers()));
  Color us = pos.side_to_move();
  return us == WHITE ? generate_potion_moves<WHITE, Type>(pos, MoveBuffer{listBegin, baseEnd})
                     : generate_potion_moves<BLACK, Type>(pos, MoveBuffer{listBegin, baseEnd});
}

// Explicit template instantiations
template ExtMove* generate<CAPTURES>(const Position&, ExtMove*);
template ExtMove* generate<QUIETS>(const Position&, ExtMove*);
template ExtMove* generate<EVASIONS>(const Position&, ExtMove*);
template ExtMove* generate<QUIET_CHECKS>(const Position&, ExtMove*);
template ExtMove* generate<NON_EVASIONS>(const Position&, ExtMove*);
template ExtMove* generate_without_potions<CAPTURES>(const Position&, ExtMove*);
template ExtMove* generate_without_potions<QUIETS>(const Position&, ExtMove*);
template ExtMove* generate_without_potions<EVASIONS>(const Position&, ExtMove*);
template ExtMove* generate_without_potions<QUIET_CHECKS>(const Position&, ExtMove*);
template ExtMove* generate_without_potions<NON_EVASIONS>(const Position&, ExtMove*);
template ExtMove* append_potions<CAPTURES>(const Position&, ExtMove*, ExtMove*);
template ExtMove* append_potions<QUIETS>(const Position&, ExtMove*, ExtMove*);
template ExtMove* append_potions<EVASIONS>(const Position&, ExtMove*, ExtMove*);
template ExtMove* append_potions<QUIET_CHECKS>(const Position&, ExtMove*, ExtMove*);
template ExtMove* append_potions<NON_EVASIONS>(const Position&, ExtMove*, ExtMove*);


/// generate<LEGAL> generates all the legal moves in the given position

template<>
ExtMove* generate<LEGAL>(const Position& pos, ExtMove* moveList) {

  if (pos.is_immediate_game_end())
      return moveList;

  ExtMove* cur = moveList;

  const bool useWrappedFallback = pos.topology_wraps() && pos.evasion_checkers();
  const bool useNonEvasions = pos.anti_royal_types() || useWrappedFallback;
  moveList = (pos.evasion_checkers() && !useNonEvasions) ? generate<EVASIONS    >(pos, moveList)
                                                         : generate<NON_EVASIONS>(pos, moveList);
  while (cur != moveList)
      if (!pos.legal(*cur) || pos.virtual_drop(*cur))
          *cur = *--moveList;
      else
          ++cur;

  return moveList;
}

#ifdef USE_HEAP_INSTEAD_OF_STACK_FOR_MOVE_LIST
template struct MoveList<LEGAL>;
#endif

} // namespace Stockfish
