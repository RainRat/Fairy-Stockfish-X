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

#include "movegen.h"
#include "position.h"

namespace Stockfish {

namespace {

  struct SpellContextGuard {
    Position& pos;
    bool active;
    bool hadContext;
    Bitboard prevFreeze;
    Bitboard prevJump;

    SpellContextGuard(const Position& position, Bitboard freezeExtra, Bitboard jumpRemoved)
        : pos(const_cast<Position&>(position)),
          active((freezeExtra | jumpRemoved) != Bitboard(0)),
          hadContext(pos.spell_context_active()),
          prevFreeze(pos.spell_freeze_extra()),
          prevJump(pos.spell_jump_removed()) {
        if (active)
            pos.set_spell_context(freezeExtra, jumpRemoved);
    }

    ~SpellContextGuard() {
        if (!active)
            return;
        if (hadContext)
            pos.set_spell_context(prevFreeze, prevJump);
        else
            pos.clear_spell_context();
    }
  };

  Bitboard useful_freeze_gates(const Position& pos, Color us) {
    Bitboard gates = 0;
    Bitboard enemies = pos.pieces(~us);
    while (enemies)
      gates |= pos.freeze_zone_from_square(pop_lsb(enemies));
    return gates;
  }

  template<MoveType T>
  ExtMove* make_move_and_gating(const Position& pos, ExtMove* moveList, Color us, Square from, Square to, PieceType pt = NO_PIECE_TYPE) {

    // Wall placing moves
    //if it's "wall or move", and they chose non-null move, skip even generating wall move
    if (pos.walling() && !(pos.wall_or_move() && (from!=to)))
    {
        Bitboard b = pos.board_bb() & ~((pos.pieces() ^ from) | to);
        if (T == CASTLING)
        {
            Square kto = make_square(to > from ? pos.castling_kingside_file() : pos.castling_queenside_file(), pos.castling_rank(us));
            Square rto = kto - (to > from ? EAST : WEST);
            b ^= square_bb(to) ^ kto ^ rto;
        }
        if (T == EN_PASSANT)
            b ^= pos.capture_square(to);

        if (pos.walling_rule() == ARROW)
            b &= moves_bb(us, type_of(pos.piece_on(from)), to, pos.pieces() ^ from);

        //Any current or future wall variant must follow the walling region rule if set:
        b &= pos.walling_region(us);

        if (pos.walling_rule() == PAST)
            b &= square_bb(from);
        if (pos.walling_rule() == EDGE)
        {
            Bitboard wallsquares = pos.state()->wallSquares;

            b &= (FileABB | file_bb(pos.max_file()) | Rank1BB | rank_bb(pos.max_rank())) |
               ( shift<NORTH     >(wallsquares) | shift<SOUTH     >(wallsquares)
               | shift<EAST      >(wallsquares) | shift<WEST      >(wallsquares));
        }
        while (b)
            *moveList++ = make_gating<T>(from, to, pt, pop_lsb(b));
        return moveList;
    }

    *moveList++ = make<T>(from, to, pt);

    // Gating moves
    if (pos.seirawan_gating() && (pos.gates(us) & from))
        for (PieceSet ps = pos.piece_types(); ps;)
        {
            PieceType pt_gating = pop_lsb(ps);
            if (pos.can_drop(us, pt_gating) && (pos.drop_region(us, pt_gating) & from))
                *moveList++ = make_gating<T>(from, to, pt_gating, from);
        }
    if (pos.seirawan_gating() && T == CASTLING && (pos.gates(us) & to))
        for (PieceSet ps = pos.piece_types(); ps;)
        {
            PieceType pt_gating = pop_lsb(ps);
            if (pos.can_drop(us, pt_gating) && (pos.drop_region(us, pt_gating) & to))
                *moveList++ = make_gating<T>(from, to, pt_gating, to);
        }

    return moveList;
  }

  template<Color c, GenType Type, Direction D>
  ExtMove* make_promotions(const Position& pos, ExtMove* moveList, Square to) {

    if (Type == CAPTURES || Type == EVASIONS || Type == NON_EVASIONS)
    {
        for (PieceSet promotions = pos.promotion_piece_types(c); promotions;)
        {
            PieceType pt = pop_msb(promotions);
            if (pos.prison_pawn_promotion() && pos.count_in_prison(~c, pt) == 0) {
                continue;
            }
            if (pos.promotion_allowed(c, pt))
                moveList = make_move_and_gating<PROMOTION>(pos, moveList, pos.side_to_move(), to - D, to, pt);
        }
        PieceType pt = pos.promoted_piece_type(PAWN);
        if (pt && !(pos.piece_promotion_on_capture() && pos.empty(to)))
            moveList = make_move_and_gating<PIECE_PROMOTION>(pos, moveList, pos.side_to_move(), to - D, to);
    }

    return moveList;
  }

  template<Color Us, GenType Type>
  ExtMove* generate_drops(const Position& pos, ExtMove* moveList, PieceType pt, Bitboard b) {
    assert(Type != CAPTURES);
    // Do not generate virtual drops for perft and at root
    if (pos.can_drop(Us, pt) || (Type != NON_EVASIONS && pos.two_boards() && pos.virtual_drops() && pos.allow_virtual_drop(Us, pt)))
    {
        // Restrict to valid target
        b &= pos.drop_region(Us, pt);

        // Add to move list
        if (pos.drop_promoted() && pos.promoted_piece_type(pt))
        {
            Bitboard b2 = b;
            if (Type == QUIET_CHECKS)
                b2 &= pos.check_squares(pos.promoted_piece_type(pt));
            while (b2)
                *moveList++ = make_drop(pop_lsb(b2), pt, pos.promoted_piece_type(pt));
        }
        if (Type == QUIET_CHECKS || !pos.can_drop(Us, pt))
            b &= pos.check_squares(pt);
        while (b)
            *moveList++ = make_drop(pop_lsb(b), pt, pt);
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
          // Restrict to valid target
          b &= pos.drop_region(Us, pt);
          // Add to move list
          if (pos.drop_promoted() && pos.promoted_piece_type(pt)) {
              Bitboard b2 = b;
              if (Type == QUIET_CHECKS)
                  b2 &= pos.check_squares(pos.promoted_piece_type(pt));
              while (b2) {
                  auto to = pop_lsb(b2);
                  for (PieceSet r = rescue; r; ) {
                      PieceType ex = pop_lsb(r);
                      *moveList++ = make_exchange(to, ex, pt, pos.promoted_piece_type(pt));
                  }
              }
          }
          if (Type == QUIET_CHECKS)
              b &= pos.check_squares(pt);
          while (b) {
              auto to = pop_lsb(b);
              for (PieceSet r = rescue; r; ) {
                  PieceType ex = pop_lsb(r);
                  *moveList++ = make_exchange(to, ex, pt, pt);
              }
          }
      }
      return moveList;
  }

  template<Color Us, GenType Type>
  ExtMove* generate_pawn_moves(const Position& pos, ExtMove* moveList, Bitboard target, Bitboard fromMask = AllSquares) {

    if (!pos.pieces(Us, PAWN))
        return moveList;

    constexpr Color     Them     = ~Us;
    constexpr Direction Up       = pawn_push(Us);
    constexpr Direction UpRight  = (Us == WHITE ? NORTH_EAST : SOUTH_WEST);
    constexpr Direction UpLeft   = (Us == WHITE ? NORTH_WEST : SOUTH_EAST);

    /// yjf2002ghty: Since it's generate_pawn_moves, I assume the piece type is PAWN. It can cause problems if the pawn is something else (e.g. Custom pawn piece)
    const Bitboard promotionZone = pos.promotion_zone(Us, PAWN);
    const Bitboard standardPromotionZone = pos.sittuyin_promotion() ? Bitboard(0) : promotionZone;
    /// yjf2002ghty: Since it's generate_pawn_moves, I assume the piece type is PAWN. It can cause problems if the pawn is something else (e.g. Custom pawn piece)
    const Bitboard doubleStepRegion = pos.double_step_region(Us, PAWN);
    /// yjf2002ghty: Since it's generate_pawn_moves, I assume the piece type is PAWN. It can cause problems if the pawn is something else (e.g. Custom pawn piece)
    const Bitboard tripleStepRegion = pos.triple_step_region(Us, PAWN);

    const Bitboard frozen     = pos.freeze_squares();
    const Bitboard pawns      = pos.pieces(Us, PAWN) & fromMask & ~frozen;
    const Bitboard neutral    = pos.dead_squares();
    const Bitboard movable    = pos.board_bb(Us, PAWN) & ~pos.pieces();
    const Bitboard friendlyCapturable = pos.pieces(Us) & ~pos.pieces(Us, KING);
    const Bitboard capturable = pos.board_bb(Us, PAWN)
                              & (pos.self_capture() ? (pos.pieces(Them) | friendlyCapturable | neutral)
                                                    :  (pos.pieces(Them) | neutral));

    target = Type == EVASIONS ? target : AllSquares;

    // Define single and double push, left and right capture, as well as respective promotion moves
    Bitboard b1 = shift<Up>(pawns) & movable & target;
    Bitboard b2 = shift<Up>(shift<Up>(pawns & doubleStepRegion) & movable) & movable & target;
    Bitboard b3 = shift<Up>(shift<Up>(shift<Up>(pawns & tripleStepRegion) & movable) & movable) & movable & target;
    Bitboard brc = shift<UpRight>(pawns) & capturable & target;
    Bitboard blc = shift<UpLeft >(pawns) & capturable & target;

    Bitboard b1p = b1 & standardPromotionZone;
    Bitboard b2p = b2 & standardPromotionZone;
    Bitboard b3p = b3 & standardPromotionZone;
    Bitboard brcp = brc & standardPromotionZone;
    Bitboard blcp = blc & standardPromotionZone;

    Bitboard mandatoryPromotionZone = pos.mandatory_promotion_zone(Us, PAWN);
    if (pos.mandatory_pawn_promotion())
        mandatoryPromotionZone |= standardPromotionZone;

    bool pawnPromotionAvailable = false;
    for (PieceSet ps = pos.promotion_piece_types(Us); ps;)
        if (pos.promotion_allowed(Us, pop_lsb(ps)))
        {
            pawnPromotionAvailable = true;
            break;
        }

    if (mandatoryPromotionZone)
    {
        b1 &= ~mandatoryPromotionZone;
        b2 &= ~mandatoryPromotionZone;
        b3 &= ~mandatoryPromotionZone;
        brc &= ~mandatoryPromotionZone;
        blc &= ~mandatoryPromotionZone;
        if (!pawnPromotionAvailable)
        {
            b1p &= ~mandatoryPromotionZone;
            b2p &= ~mandatoryPromotionZone;
            b3p &= ~mandatoryPromotionZone;
            brcp &= ~mandatoryPromotionZone;
            blcp &= ~mandatoryPromotionZone;
        }
    }

    if (Type == QUIET_CHECKS && pos.count<KING>(Them))
    {
        // To make a quiet check, you either make a direct check by pushing a pawn
        // or push a blocker pawn that is not on the same file as the enemy king.
        // Discovered check promotion has been already generated amongst the captures.
        Square ksq = pos.square<KING>(Them);
        Bitboard dcCandidatePawns = pos.blockers_for_king(Them) & ~file_bb(ksq);
        b1 &= pawn_attacks_bb(Them, ksq) | shift<   Up>(dcCandidatePawns);
        b2 &= pawn_attacks_bb(Them, ksq) | shift<Up+Up>(dcCandidatePawns);
    }

    // Single and double pawn pushes, no promotions
    if (Type != CAPTURES)
    {
        while (b1)
        {
            Square to = pop_lsb(b1);
            moveList = make_move_and_gating<NORMAL>(pos, moveList, Us, to - Up, to);
        }

        while (b2)
        {
            Square to = pop_lsb(b2);
            moveList = make_move_and_gating<NORMAL>(pos, moveList, Us, to - Up - Up, to);
        }

        while (b3)
        {
            Square to = pop_lsb(b3);
            moveList = make_move_and_gating<NORMAL>(pos, moveList, Us, to - Up - Up - Up, to);
        }
    }

    // Promotions and underpromotions
    while (brcp)
        moveList = make_promotions<Us, Type, UpRight>(pos, moveList, pop_lsb(brcp));

    while (blcp)
        moveList = make_promotions<Us, Type, UpLeft >(pos, moveList, pop_lsb(blcp));

    while (b1p)
        moveList = make_promotions<Us, Type, Up     >(pos, moveList, pop_lsb(b1p));

    while (b2p)
        moveList = make_promotions<Us, Type, Up+Up  >(pos, moveList, pop_lsb(b2p));

    while (b3p)
        moveList = make_promotions<Us, Type, Up+Up+Up>(pos, moveList, pop_lsb(b3p));

    // Sittuyin promotions
    if (pos.sittuyin_promotion() && (Type == CAPTURES || Type == EVASIONS || Type == NON_EVASIONS))
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
                Bitboard b = ((pos.attacks_from(Us, pt, from) & ~(pos.pieces() | pos.dead_squares())) | from) & target;
                while (b)
                {
                    Square to = pop_lsb(b);
                    if (!(attacks_bb(Us, pt, to, pos.pieces() ^ from) & pos.pieces(Them)))
                        *moveList++ = make<PROMOTION>(from, to, pt);
                }
            }
        }
    }

    // Standard and en passant captures
    if (Type == CAPTURES || Type == EVASIONS || Type == NON_EVASIONS)
    {
        while (brc)
        {
            Square to = pop_lsb(brc);
            moveList = make_move_and_gating<NORMAL>(pos, moveList, Us, to - UpRight, to);
        }

        while (blc)
        {
            Square to = pop_lsb(blc);
            moveList = make_move_and_gating<NORMAL>(pos, moveList, Us, to - UpLeft, to);
        }

        for (Bitboard epSquares = pos.ep_squares() & ~(pos.pieces() | pos.dead_squares()); epSquares; )
        {
            Square epSquare = pop_lsb(epSquares);

            // An en passant capture cannot resolve a discovered check (unless there non-sliding riders)
            if (Type == EVASIONS && (target & (epSquare + Up)) && !pos.non_sliding_riders())
                return moveList;

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
  ExtMove* generate_moves(const Position& pos, ExtMove* moveList, PieceType Pt, Bitboard target, Bitboard captureTarget, Bitboard fromMask = AllSquares) {

    assert(Pt != KING && Pt != PAWN);

    Bitboard bb = pos.pieces(Us, Pt) & fromMask;
    Bitboard frozen = pos.freeze_squares();

    while (bb)
    {
        Square from = pop_lsb(bb);

        if (frozen & from)
            continue;

        Bitboard attacks = pos.attacks_from(Us, Pt, from);
        Bitboard quiets = pos.moves_from(Us, Pt, from);
        Bitboard captureSquares = (attacks & pos.pieces()) & captureTarget;
        Bitboard quietSquares   = (quiets & ~pos.pieces()) & target;
        Bitboard b = captureSquares | quietSquares;
        Bitboard epSquares = (pos.en_passant_types(Us) & Pt) ? (attacks & pos.ep_squares() & ~pos.pieces()) : Bitboard(0);
        Bitboard b1 = b & ~epSquares;
        Bitboard promotion_zone = pos.promotion_zone(Us, Pt);
        Bitboard mandatoryPromotionZone = pos.mandatory_promotion_zone(Us, Pt);
        PieceType promPt = pos.promoted_piece_type(Pt);
        Bitboard b2 = promPt && pos.promotion_allowed(Us, promPt) ? b1 : Bitboard(0);
        Bitboard b3 = pos.piece_demotion() && pos.is_promoted(from) ? b1 : Bitboard(0);
        Bitboard pawnPromotions = (pos.promotion_pawn_types(Us) & Pt)
                                ? (b & (Type == EVASIONS ? target : (~pos.pieces(Us) | (pos.self_capture() ? (pos.pieces(Us) & ~pos.pieces(Us, KING)) : Bitboard(0)))) & promotion_zone)
                                : Bitboard(0);
        Bitboard jumpCaptures = 0;
        PieceSet jumpTypes = pos.jump_capture_types();
        if ((jumpTypes & ALL_PIECES) || (jumpTypes & Pt))
        {
            Bitboard candidates = (attacks | quiets) & ~pos.pieces();
            while (candidates)
            {
                Square to = pop_lsb(candidates);
                if (pos.jump_capture_square(from, to) != SQ_NONE)
                    jumpCaptures |= to;
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

        // Jump captures are emitted explicitly below in capture-generating modes.
        // Exclude them from regular NORMAL generation to avoid duplicates.
        b1 &= ~jumpCaptures;

        while (b1)
            moveList = make_move_and_gating<NORMAL>(pos, moveList, Us, from, pop_lsb(b1));

        // Shogi-style piece promotions
        while (b2)
            *moveList++ = make<PIECE_PROMOTION>(from, pop_lsb(b2));

        // Piece demotions
        while (b3)
            *moveList++ = make<PIECE_DEMOTION>(from, pop_lsb(b3));

        // Pawn-style promotions
        if ((Type == CAPTURES || Type == QUIETS || Type == EVASIONS || Type == NON_EVASIONS) && pawnPromotions)
            for (PieceSet ps = pos.promotion_piece_types(Us); ps;)
            {
                PieceType ptP = pop_msb(ps);
                if (pos.prison_pawn_promotion() && pos.count_in_prison(~Us, ptP) == 0) {
                    continue;
                }
                if (pos.promotion_allowed(Us, ptP))
                    for (Bitboard promotions = pawnPromotions; promotions; )
                        moveList = make_move_and_gating<PROMOTION>(pos, moveList, pos.side_to_move(), from, pop_lsb(promotions), ptP);
            }

        // En passant captures
        if (Type == CAPTURES || Type == EVASIONS || Type == NON_EVASIONS)
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
    const Square ksq = pos.count<KING>(Us) ? pos.square<KING>(Us) : SQ_NONE;
    const Bitboard checkers = pos.checkers();
    Bitboard target;
    Bitboard captureTarget = Bitboard(0);
    Bitboard forcedFromMask = AllSquares;
    bool restrictToForcedJumper = false;
    Bitboard jumpForbidden = pos.spell_jump_removed();

    Square forcedSquare = pos.forced_jump_square();
    if (forcedSquare != SQ_NONE && pos.has_forced_jump_followup())
    {
        Piece forcedPiece = pos.piece_on(forcedSquare);
        if (forcedPiece != NO_PIECE) {
            if (color_of(forcedPiece) == Us)
            {
                restrictToForcedJumper = true;
                forcedFromMask = square_bb(forcedSquare);
            }
            else
            {
                // Opponent must pass while the other side completes a forced jump chain.
                if (pos.pass(Us) && pos.pieces(Us))
                {
                    Bitboard usPieces = pos.pieces(Us);
                    Square passSq = lsb(usPieces);
                    *moveList++ = make<SPECIAL>(passSq, passSq);
                }
                return moveList;
            }
        }
    }

    // Skip generating non-king moves when in double check
    if (Type != EVASIONS || !more_than_one(checkers & ~pos.non_sliding_riders()))
    {
        target = Type == EVASIONS     ?  between_bb(ksq, lsb(checkers))
               : Type == NON_EVASIONS ? ~pos.pieces( Us)
               : Type == CAPTURES     ? (pos.pieces(~Us) | pos.dead_squares())
                                      : ~pos.pieces(   ); // QUIETS || QUIET_CHECKS

        if (Type == EVASIONS)
        {
            const bool multipleCheckers = more_than_one(checkers);
            Square checksq = lsb(checkers);
            PieceType checkerPt = type_of(pos.piece_on(checksq));

            if (multipleCheckers)
                target = checkers;
            else
                target = between_bb(ksq, checksq, checkerPt);

            bool blockableNightrider = (AttackRiderTypes[checkerPt] & RIDER_NIGHTRIDER) && !multipleCheckers;
            if ((checkers & pos.non_sliding_riders()) && !blockableNightrider)
                target = ~pos.pieces(Us);
            // Leaper attacks can not be blocked
            if (LeaperAttacks[~Us][type_of(pos.piece_on(checksq))][checksq] & pos.square<KING>(Us))
                target = checkers;
        }

        // Remove inaccessible squares (outside board + wall squares)
        target &= pos.board_bb() & ~jumpForbidden;

        captureTarget = target;
        if (pos.self_capture() && (Type == NON_EVASIONS || Type == CAPTURES || Type == EVASIONS))
        {
            Bitboard selfCaptureTargets = pos.pieces(Us) & ~pos.pieces(Us, KING);
            // During check evasions, only consider self-captures that can
            // actually resolve the check (capture checker or block line).
            if (Type == EVASIONS)
                selfCaptureTargets &= target;
            captureTarget |= selfCaptureTargets;
        }

        // During forced jump continuation, only jump captures from the forced
        // piece are legal. Suppress regular quiet/capture generation here and
        // let explicit jump-capture emission paths produce candidates.
        if (restrictToForcedJumper)
        {
            target = Bitboard(0);
            captureTarget = Bitboard(0);
        }

        if (restrictToForcedJumper)
        {
            PieceType forcedPt = type_of(pos.piece_on(forcedSquare));
            if (forcedPt == PAWN)
                moveList = generate_pawn_moves<Us, Type>(pos, moveList, target, forcedFromMask);
            else if (forcedPt != KING)
                moveList = generate_moves<Us, Type>(pos, moveList, forcedPt, target, captureTarget, forcedFromMask);
        }
        else
        {
            moveList = generate_pawn_moves<Us, Type>(pos, moveList, target, forcedFromMask);
            for (PieceSet ps = pos.piece_types() & ~(piece_set(PAWN) | KING); ps;)
                moveList = generate_moves<Us, Type>(pos, moveList, pop_lsb(ps), target, captureTarget, forcedFromMask);
        }
        // generate drops
        if (!restrictToForcedJumper && pos.piece_drops() && Type != CAPTURES && (pos.can_drop(Us, ALL_PIECES) || pos.two_boards()))
            for (PieceSet ps = pos.piece_types(); ps;)
                moveList = generate_drops<Us, Type>(pos, moveList, pop_lsb(ps), target & ~pos.pieces(~Us));
        // generate exchange
        if (!restrictToForcedJumper && pos.capture_type() == PRISON && Type != CAPTURES && pos.has_exchange())
            for (PieceSet ps = pos.piece_types(); ps;)
                moveList = generate_exchanges<Us, Type>(pos, moveList, pop_lsb(ps), target & ~pos.pieces(~Us));

        // Castling with non-king piece
        if constexpr (Type != CAPTURES)
            if (!restrictToForcedJumper && !pos.count<KING>(Us) && pos.can_castle(Us & ANY_CASTLING))
            {
                Square from = pos.castling_king_square(Us);
                for(CastlingRights cr : { Us & KING_SIDE, Us & QUEEN_SIDE } )
                    if (!pos.castling_impeded(cr) && pos.can_castle(cr))
                        moveList = make_move_and_gating<CASTLING>(pos, moveList, Us, from, pos.castling_rook_square(cr));
            }

        // Special moves
        if constexpr (Type != CAPTURES)
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

        // Workaround for passing: Execute a non-move with any piece
        if (!restrictToForcedJumper && pos.pass(Us) && !pos.count<KING>(Us) && pos.pieces(Us))
        {
            Bitboard usPieces = pos.pieces(Us);
            Square passSq = lsb(usPieces);
            *moveList++ = make<SPECIAL>(passSq, passSq);
        }

        //if "wall or move", generate walling action with null move
        if (!restrictToForcedJumper && pos.wall_or_move())
        {
            moveList = make_move_and_gating<SPECIAL>(pos, moveList, Us, lsb(pos.pieces(Us)), lsb(pos.pieces(Us)));
        }
    }

    // King moves
    if (pos.count<KING>(Us) && (!restrictToForcedJumper || (forcedFromMask & ksq)) && (!Checks || pos.blockers_for_king(~Us) & ksq))
    {
        Bitboard kingAttacks = pos.attacks_from(Us, KING, ksq) & pos.pieces();
        Bitboard kingMoves   = pos.moves_from(Us, KING, ksq) & ~pos.pieces();
        Bitboard kingCaptureMask = Type == EVASIONS ? ~pos.pieces(Us) : captureTarget;
        if (Type == EVASIONS && pos.self_capture())
            kingCaptureMask |= pos.pieces(Us) & ~pos.pieces(Us, KING);
        Bitboard kingQuietMask = Type == EVASIONS ? ~pos.pieces(Us) : target;
        Bitboard b = (kingAttacks & kingCaptureMask) | (kingMoves & kingQuietMask);
        while (b)
            moveList = make_move_and_gating<NORMAL>(pos, moveList, Us, ksq, pop_lsb(b));

        // Passing move by king
        if (!restrictToForcedJumper && pos.pass(Us))
            *moveList++ = make<SPECIAL>(ksq, ksq);

        if (!restrictToForcedJumper && (Type == QUIETS || Type == NON_EVASIONS) && pos.can_castle(Us & ANY_CASTLING))
            for (CastlingRights cr : { Us & KING_SIDE, Us & QUEEN_SIDE } )
                if (!pos.castling_impeded(cr) && pos.can_castle(cr))
                    moveList = make_move_and_gating<CASTLING>(pos, moveList, Us,ksq, pos.castling_rook_square(cr));
    }

    return moveList;
  }

  template<Color Us, GenType Type>
  ExtMove* generate_potion_moves(const Position& pos, ExtMove* listBegin, ExtMove* baseEnd) {
    const Variant* var = pos.variant();
    ExtMove* cur = baseEnd;
    ExtMove* maxEnd = listBegin + MOVEGEN_OVERFLOW_CAPACITY;

    for (int pt = 0; pt < Variant::POTION_TYPE_NB; ++pt)
    {
        auto potion = static_cast<Variant::PotionType>(pt);
        PieceType potionPiece = pos.potion_piece(potion);
        if (potionPiece == NO_PIECE_TYPE)
            continue;
        if (!pos.can_cast_potion(Us, potion))
            continue;

        ExtMove* freezeBaseEnd = baseEnd;
        if (potion == Variant::POTION_FREEZE)
        {
            // Build a compact source list once; reused for each freeze gate.
            freezeBaseEnd = listBegin;
            for (ExtMove* it = listBegin; it != baseEnd; ++it)
            {
                Move base = it->move;
                MoveType mt = type_of(base);
                if (is_gating(base) || (mt != NORMAL && mt != CASTLING))
                    continue;
                *freezeBaseEnd++ = *it;
            }

            if (freezeBaseEnd == listBegin)
                continue;
        }

        Bitboard candidates = pos.board_bb();
        if (!var->potionDropOnOccupied)
            candidates &= ~pos.pieces();

        if (potion == Variant::POTION_FREEZE)
            candidates &= useful_freeze_gates(pos, Us);
        else if (potion == Variant::POTION_JUMP)
            candidates &= pos.pieces();

        if (potion == Variant::POTION_FREEZE)
        {
            while (candidates)
            {
                if (cur >= maxEnd)
                    return maxEnd;

                Square gate = pop_lsb(candidates);
                for (ExtMove* it = listBegin; it != freezeBaseEnd; ++it)
                {
                    if (cur >= maxEnd)
                        return maxEnd;

                    Move base = it->move;
                    MoveType mt = type_of(base);
                    Square from = from_sq(base);
                    Square to = to_sq(base);

                    Move gatingMove = mt == NORMAL
                                      ? make_gating<NORMAL>(from, to, potionPiece, gate)
                                      : make_gating<CASTLING>(from, to, potionPiece, gate);

                    cur->move = gatingMove;
                    cur->value = it->value;
                    ++cur;
                }
            }
            continue;
        }

        while (candidates)
        {
            if (cur >= maxEnd)
                return maxEnd;

            Square gate = pop_lsb(candidates);
            assert(potion == Variant::POTION_JUMP);

            Bitboard gateMask = square_bb(gate);
            SpellContextGuard guard(pos, Bitboard(0), gateMask);

            ExtMove* potionStart = cur;
            cur = generate_all_impl<Us, Type>(pos, cur);

            ExtMove* write = potionStart;
            for (ExtMove* it = potionStart; it != cur; ++it)
            {
                if (write >= maxEnd)
                    return maxEnd;

                Move base = it->move;
                if (is_gating(base))
                    continue;

                MoveType mt = type_of(base);
                if (mt != NORMAL && mt != CASTLING)
                    continue;
                Square from = from_sq(base);
                Square to = to_sq(base);

                Piece mover = pos.piece_on(from);
                if (mover == NO_PIECE)
                    continue;

                PieceType moverType = type_of(mover);
                // Pure leapers cannot have an intermediate path square.
                if (mt == NORMAL
                    && AttackRiderTypes[moverType] == NO_RIDER
                    && moverType != PAWN
                    && moverType != SHOGI_PAWN
                    && moverType != SOLDIER)
                    continue;

                if (to == gate)
                    continue;

                if (distance(from, to) <= 1)
                    continue;

                Bitboard path = between_bb(from, to, moverType);
                if (!(path & gateMask))
                    continue;

                Move gatingMove = mt == NORMAL
                                  ? make_gating<NORMAL>(from, to, potionPiece, gate)
                                  : make_gating<CASTLING>(from, to, potionPiece, gate);

                write->move = gatingMove;
                write->value = it->value;
                ++write;
            }

            cur = write;
        }
    }

    return cur;
  }

  template<Color Us, GenType Type>
  ExtMove* generate_all(const Position& pos, ExtMove* moveList) {

    ExtMove* baseEnd = generate_all_impl<Us, Type>(pos, moveList);
    if (!pos.potions_enabled())
        return baseEnd;
    if (!pos.can_cast_potion(Us, Variant::POTION_FREEZE)
        && !pos.can_cast_potion(Us, Variant::POTION_JUMP))
        return baseEnd;
    return generate_potion_moves<Us, Type>(pos, moveList, baseEnd);
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
  assert((Type == EVASIONS) == (bool)pos.checkers());

  Color us = pos.side_to_move();

  return us == WHITE ? generate_all<WHITE, Type>(pos, moveList)
                     : generate_all<BLACK, Type>(pos, moveList);
}

// Explicit template instantiations
template ExtMove* generate<CAPTURES>(const Position&, ExtMove*);
template ExtMove* generate<QUIETS>(const Position&, ExtMove*);
template ExtMove* generate<EVASIONS>(const Position&, ExtMove*);
template ExtMove* generate<QUIET_CHECKS>(const Position&, ExtMove*);
template ExtMove* generate<NON_EVASIONS>(const Position&, ExtMove*);


/// generate<LEGAL> generates all the legal moves in the given position

template<>
ExtMove* generate<LEGAL>(const Position& pos, ExtMove* moveList) {

  if (pos.is_immediate_game_end())
      return moveList;

  ExtMove* cur = moveList;

  moveList = pos.checkers() ? generate<EVASIONS    >(pos, moveList)
                            : generate<NON_EVASIONS>(pos, moveList);
  while (cur != moveList)
      if (!pos.legal(*cur) || pos.virtual_drop(*cur))
          *cur = (--moveList)->move;
      else
          ++cur;

  return moveList;
}

} // namespace Stockfish
