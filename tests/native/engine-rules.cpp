#include <algorithm>
#include <cstdint>
#include <deque>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "apiutil.h"
#include "test-support.hpp"
#ifdef ENABLE_COMPOUND_TURNS
#include "compound_turn.h"
#endif

using namespace Stockfish;

namespace Stockfish::Zobrist {
extern Key psq[PIECE_NB][SQUARE_NB];
extern Key noPawns;
}

namespace {

using Test = std::function<void()>;

[[noreturn]] void fail(const std::string& message) {
    throw std::runtime_error(message);
}

void check(bool condition, const std::string& message) {
    if (!condition)
        fail(message);
}

void set_position(Position& pos, StateListPtr& states, const char* variant, const char* fen) {
    const Variant* v = variants.get(variant);
    check(v != nullptr, std::string("required variant is missing: ") + variant);
    UCI::init_variant(v);
    states = StateListPtr(new std::deque<StateInfo>(1));
    pos.set(v, fen, false, &states->back(), nullptr);
}

Move parse_move(const Position& pos, const char* notation) {
    std::string text(notation);
    Move move = UCI::to_move(pos, text);
    check(move != MOVE_NONE, std::string("failed to parse move: ") + notation);
    return move;
}

void check_simulation_matches_move(Position& pos, StateListPtr& states, Move move,
                                   const std::string& context) {
    SimulatedMoveInfo simulated = pos.simulated_move_info(move);
    std::vector<std::pair<Square, Piece>> expectedPieces;
    {
        Position::SimulatedMoveInfoGuard guard(pos);
        guard.set(simulated);
        Bitboard occupied = simulated.occupiedAfterEffects;
        while (occupied)
        {
            Square sq = pop_lsb(occupied);
            expectedPieces.emplace_back(sq, pos.piece_at(sq, simulated.occupiedAfterEffects));
        }
    }

    const Key beforeKey = pos.key();
    const std::string beforeFen = pos.fen();
    states->emplace_back();
    pos.do_move(move, states->back());
    check(pos.pieces() == simulated.occupiedAfterEffects,
          context + " occupancy disagreed with move application");
    for (const auto& [sq, piece] : expectedPieces)
        check(pos.piece_on(sq) == piece,
              context + " piece identity disagreed with move application");
    pos.undo_move(move);
    states->pop_back();
    check(pos.key() == beforeKey && pos.fen() == beforeFen,
          context + " did not restore state after parity check");
}

Value public_game_result(Position& pos) {
    Value result = VALUE_NONE;
    if (pos.is_immediate_game_end(result))
        return result;
    if (has_insufficient_material(WHITE, pos) && has_insufficient_material(BLACK, pos))
        return VALUE_DRAW;
    if (pos.is_optional_game_end(result))
        return result;
    if (MoveList<LEGAL>(pos).size() == 0)
        return pos.evasion_checkers() ? pos.checkmate_value() : pos.stalemate_value();
    return VALUE_NONE;
}

void promotion() {
    Position pos;
    StateListPtr states;
    set_position(pos, states, "chess",
                 "rnbqkbnr/1ppppppp/8/8/8/8/pPPPPPPP/RNBQKBNR w KQkq - 0 1");
    check(pos.promotion_square(WHITE, SQ_A2) == SQ_NONE,
          "promotion_square accepted a mismatched-color pawn");

    set_position(pos, states, "chess",
                 "rnbqkbnr/P1pppppp/8/8/8/8/1PPPPPPP/RNBQKBNR w KQkq - 0 1");
    check(pos.promotion_square(WHITE, SQ_A7) == SQ_A8,
          "promotion_square failed to find the promotion square");
}

void movement() {
    Position pos;
    StateListPtr states;
    set_position(pos, states, "chess",
                 "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    check(!pos.clone_targets_from(WHITE, SQ_A3),
          "clone_targets_from returned targets for an empty square");
}

void composable_rules() {
    Position pos;
    StateListPtr states;

    set_position(pos, states, "composable-freeze-traps",
                 "8/8/8/4e3/4M3/8/7r/8 w - - 0 1");
    check(pos.freeze_squares() & square_bb(SQ_E4), "adjacent freezer mask missed the mover");
    check(pos.see_pruning_unreliable(),
          "SEE pruning remained enabled with static freezing");
    Move frozen = make_move(SQ_E4, SQ_E3);
    check(!pos.legal(frozen), "adjacent freezer did not freeze the mover");

    set_position(pos, states, "composable-freeze-traps",
                 "8/8/8/3e4/4M3/8/7r/8 w - - 0 1");
    check(!pos.legal(make_move(SQ_E4, SQ_E3)),
          "diagonal freezer did not freeze the mover");

    set_position(pos, states, "composable-freeze-orth-only",
                 "8/8/8/3e4/4M3/8/7r/8 w - - 0 1");
    check(pos.legal(make_move(SQ_E4, SQ_E3)),
          "orthogonal-only freezer unexpectedly froze diagonally");

    set_position(pos, states, "composable-freeze-traps",
                 "8/8/8/8/4e3/4M3/7r/8 w - - 0 1");
    check(pos.see_pruning_unreliable(),
          "SEE pruning remained enabled with trap effects");

    set_position(pos, states, "composable-freeze-immune",
                 "8/8/8/4e3/4M3/8/7r/8 w - - 0 1");
    check(pos.legal(make_move(SQ_E4, SQ_E3)),
          "freeze-immune piece was frozen");

    set_position(pos, states, "composable-pass-freeze",
                 "8/8/8/8/8/8/8/Ff6 w - - 0 1");
    Move frozenAnchorPass = make<SPECIAL>(SQ_A1, SQ_A1);
    check(pos.legal(frozenAnchorPass),
          "a frozen pass anchor incorrectly made passing illegal");
    check(MoveList<LEGAL>(pos).contains(frozenAnchorPass),
          "a frozen pass anchor was not generated");

    set_position(pos, states, "composable-freeze-wall",
                 "8/8/8/8/8/8/f7/F7 w - - 0 1");
    Move frozenAnchorWall = make_gating<SPECIAL>(SQ_A1, SQ_A1, NO_PIECE_TYPE, SQ_H8);
    check(pos.freeze_squares() & square_bb(SQ_A1),
          "pure wall test did not freeze the generated anchor");
    check(pos.pseudo_legal(frozenAnchorWall),
          "a frozen anchor incorrectly made a pure wall move pseudo-illegal");
    check(pos.legal(frozenAnchorWall),
          "a frozen anchor incorrectly made a pure wall move illegal");
    check(MoveList<LEGAL>(pos).contains(frozenAnchorWall),
          "a pure wall move with a frozen generated anchor was not generated");

    set_position(pos, states, "composable-freeze-traps",
                 "8/8/8/4m3/4M3/8/7r/8 w - - 0 1");
    Move nonFreezingPiece = parse_move(pos, "e4e3");
    check(pos.legal(nonFreezingPiece), "a non-freezer incorrectly froze the mover");

    set_position(pos, states, "composable-freeze-traps",
                 "8/8/8/4e3/4E3/8/7r/8 w - - 0 1");
    Move mutualFreeze = make_move(SQ_E4, SQ_E3);
    check(!pos.legal(mutualFreeze), "adjacent freezers did not freeze each other");

    set_position(pos, states, "composable-freeze-traps",
                 "8/8/8/8/8/8/2R4r/8 w - - 0 1");
    Move unprotectedTrap = parse_move(pos, "c2c3");
    SimulatedMoveInfo simulated = pos.simulated_move_info(unprotectedTrap);
    check(!(simulated.occupiedAfterEffects & square_bb(SQ_C3)),
          "unprotected trap occupant survived move simulation");
    const Key beforeTrap = pos.key();
    const std::string beforeTrapFen = pos.fen();
    states->emplace_back();
    pos.do_move(unprotectedTrap, states->back());
    check(pos.piece_on(SQ_C3) == NO_PIECE,
          "unprotected trap occupant survived move application");
    pos.undo_move(unprotectedTrap);
    states->pop_back();
    check(pos.key() == beforeTrap && pos.fen() == beforeTrapFen,
          "trap removal did not restore state exactly on undo");

    set_position(pos, states, "composable-freeze-traps",
                 "8/8/8/8/8/8/2R4r/8 w - - 99 1");
    Move trapRule50 = parse_move(pos, "c2c3");
    states->emplace_back();
    pos.do_move(trapRule50, states->back());
    check(pos.state()->rule50 == 0,
          "trap removal did not reset the halfmove clock");
    pos.undo_move(trapRule50);
    states->pop_back();

    set_position(pos, states, "composable-trap-ep-stale",
                 "4k3/p7/8/1P6/8/8/8/4K3 b - - 0 1");
    Move trapDoubleStep = parse_move(pos, "a7a5");
    const Key beforeTrapDoubleStep = pos.key();
    const std::string beforeTrapDoubleStepFen = pos.fen();
    states->emplace_back();
    pos.do_move(trapDoubleStep, states->back());
    check(!pos.ep_squares(),
          "trap removal left an en-passant right for a removed pawn");
    check(!MoveList<LEGAL>(pos).contains(make<EN_PASSANT>(SQ_B5, SQ_A6)),
          "move generation exposed en passant for a trapped piece");
    pos.undo_move(trapDoubleStep);
    states->pop_back();
    check(pos.key() == beforeTrapDoubleStep && pos.fen() == beforeTrapDoubleStepFen,
          "trap en-passant cleanup did not restore state exactly on undo");

    set_position(pos, states, "composable-trap-claim-undo",
                 "4k3/8/8/4R3/3R1R2/2N1R3/8/4K3 w - - 0 1");
    Move trapClaim = make<NORMAL>(SQ_C3, SQ_E4);
    check_simulation_matches_move(pos, states, trapClaim, "trap/claim simulation");
    const Key beforeTrapClaim = pos.key();
    const std::string beforeTrapClaimFen = pos.fen();
    states->emplace_back();
    pos.do_move(trapClaim, states->back());
    check(pos.state()->trapRemoved & square_bb(SQ_E4),
          "trap/claim test did not remove the trapped piece");
    check(pos.state()->claimedSquares & square_bb(SQ_E4),
          "trap/claim test did not create the surrounding claim");
    pos.undo_move(trapClaim);
    states->pop_back();
    check(pos.key() == beforeTrapClaim && pos.fen() == beforeTrapClaimFen,
          "trap and surround claim did not restore state exactly on undo");

    set_position(pos, states, "composable-trap-claim-royal",
                 "k7/8/8/4p3/3pKp2/4p3/8/R7 w - - 0 1");
    Move trapRoyalClaim = make_move(SQ_A1, SQ_A2);
    SimulatedMoveInfo trapRoyalInfo = pos.simulated_move_info(trapRoyalClaim);
    {
        Position::SimulatedMoveInfoGuard view(pos);
        view.set(trapRoyalInfo);
        check(pos.piece_at(SQ_E4, trapRoyalInfo.occupiedAfterEffects) == make_piece(WHITE, PAWN),
              "trap/claim simulation did not replace the royal with the claim piece");
    }
    check(!pos.legal(trapRoyalClaim),
          "trap/claim replacement allowed a trapped royal to survive as a pawn");
    check(!MoveList<LEGAL>(pos).contains(trapRoyalClaim),
          "move generation retained a trap/claim replacement of the royal");

    set_position(pos, states, "composable-laser-freeze",
                 "8k/9/9/9/3rR(0)4/9/R8/K8 w - - 0 1");
    Move frozenSecondaryRotation = make_rotation<NORMAL>(SQ_A2, SQ_A3, 1, SQ_E5);
    check(pos.freeze_squares() & square_bb(SQ_E5),
          "laser secondary-rotation test did not freeze the rotator");
    check(!pos.pseudo_legal(frozenSecondaryRotation),
          "pseudo-legal accepted a rotation of a frozen secondary piece");
    check(!pos.legal(frozenSecondaryRotation),
          "laser move rotated a frozen secondary piece");
    check(!MoveList<LEGAL>(pos).contains(frozenSecondaryRotation),
          "move generation retained a rotation of a frozen secondary piece");

    set_position(pos, states, "composable-check-projected-freeze",
                 "8/8/8/3q3k/8/8/8/4R2K w - - 0 1");
    Move frozenCheckingRook = parse_move(pos, "e1e4");
    check(!pos.gives_check(frozenCheckingRook),
          "gives_check counted a checker frozen by a post-move freezer");
    check(pos.legal(frozenCheckingRook),
          "a post-move-frozen checker was incorrectly rejected as a check");

    set_position(pos, states, "composable-sacred-static-freeze",
                 "7k/8/8/8/8/8/3q4/4K3 w - - 0 1");
    check(!(pos.freeze_squares() & square_bb(SQ_E1)),
          "checked static royal did not ignore freeze");
    check(pos.legal(make_move(SQ_E1, SQ_F1)),
          "checked static royal remained immobilized by freeze");

    set_position(pos, states, "composable-sacred-static-freeze",
                 "k7/8/8/8/8/8/4q3/4K3 w - - 0 1");
    Move movedSacredRoyal = make_move(SQ_E1, SQ_F1);
    simulated = pos.simulated_move_info(movedSacredRoyal);
    check(!(pos.freeze_squares(WHITE, &simulated) & square_bb(SQ_F1)),
          "projected checked royal remained frozen at its destination");

    set_position(pos, states, "chess",
                 "R3k2K/8/8/8/8/8/8/8 w - - 0 1");
    Move captureEnemyKing = make_move(SQ_A8, SQ_E8);
    check(!pos.legal(captureEnemyKing),
          "simple legality allowed capturing the opposing king");

    set_position(pos, states, "composable-commitgate-castle",
                 "n7/4k3/8/8/8/8/8/8/4K2R/4R2R w K - 0 1");
    Move committedCastle = parse_move(pos, "e1g1");
    SimulatedMoveInfo castlingInfo = pos.simulated_move_info(committedCastle);
    check(castlingInfo.type_pieces(WHITE, ROOK) & square_bb(SQ_E1),
          "castling simulation missed the king-source committed gate");
    check(castlingInfo.type_pieces(WHITE, ROOK) & square_bb(SQ_H1),
          "castling simulation missed the rook-source committed gate");
    {
        Position::SimulatedMoveInfoGuard view(pos);
        view.set(castlingInfo);
        check(pos.piece_at(SQ_E1, castlingInfo.occupiedAfterEffects) == make_piece(WHITE, ROOK),
              "castling simulation hid the king-source committed gate identity");
        check(pos.piece_at(SQ_H1, castlingInfo.occupiedAfterEffects) == make_piece(WHITE, ROOK),
              "castling simulation hid the rook-source committed gate identity");
    }
    check_simulation_matches_move(pos, states, committedCastle,
                                  "committed-gate castling simulation");

    set_position(pos, states, "composable-commitgate-castle-blast",
                 "n7/4k3/8/8/8/8/8/8/4K2R/4R2R w K - 0 1");
    Move blastCastle = parse_move(pos, "e1g1");
    SimulatedMoveInfo blastCastlingInfo = pos.simulated_move_info(blastCastle);
    check(blastCastlingInfo.type_pieces(WHITE, QUEEN) & square_bb(SQ_H1),
          "blast promotion missed the rook-source committed gate identity");
    check_simulation_matches_move(pos, states, blastCastle,
                                  "blast-promoted committed-gate castling simulation");

    set_position(pos, states, "composable-commitgate-castle",
                 "8/8/8/8/8/8/8/8/r3R2K/4R3 w - - 0 1");
    Move committedBlock = make_move(SQ_E1, SQ_E2);
    check(pos.legal(committedBlock),
          "simple legality ignored a committed gate that still blocks check");
    states->emplace_back();
    pos.do_move(committedBlock, states->back());
    check(pos.piece_on(SQ_E1) == make_piece(WHITE, ROOK),
          "committed gate was not restored on the vacated source square");
    pos.undo_move(committedBlock);
    states->pop_back();

    set_position(pos, states, "composable-freeze-traps",
                 "8/8/8/8/8/1R6/2R4r/8 w - - 0 1");
    Move protectedTrap = parse_move(pos, "c2c3");
    simulated = pos.simulated_move_info(protectedTrap);
    check(simulated.occupiedAfterEffects & square_bb(SQ_C3),
          "friendly adjacent piece did not protect trap occupant");

    set_position(pos, states, "composable-freeze-traps-blast",
                 "8/8/8/4e3/8/8/4E3/8 w - - 0 1");
    Move blastMove = make_move(SQ_E2, SQ_E3);
    simulated = pos.simulated_move_info(blastMove);
    check(!(simulated.occupiedAfterEffects & square_bb(SQ_E4)),
          "blast-promoted trap occupant survived move simulation");
    states->emplace_back();
    pos.do_move(blastMove, states->back());
    check(pos.piece_on(SQ_E4) == NO_PIECE,
          "blast-promoted trap occupant survived move application");
    pos.undo_move(blastMove);
    states->pop_back();

    set_position(pos, states, "composable-trap-blocker",
                 "8/8/8/8/8/2*5/2R5/8 w - - 0 1");
    SimulatedMoveInfo blockerInfo = pos.simulated_move_info(make_move(SQ_C2, SQ_C1));
    check((blockerInfo.occupiedAfterEffects & square_bb(SQ_C3))
          && !(blockerInfo.removedByEffects & square_bb(SQ_C3)),
          "trap simulation treated a wall as a removable piece");

    set_position(pos, states, "composable-freeze-check",
                 "4k3/8/8/8/8/8/3Fr3/4K3 w - - 0 1");
    check(!(pos.attackers_to_king(SQ_E1, BLACK) & square_bb(SQ_E2)),
          "frozen enemy piece still counted as a checker");
    check(!pos.legal(make_move(SQ_D2, SQ_C3)),
          "moving a freezer away incorrectly left the king safe");

    set_position(pos, states, "composable-freeze-check",
                 "k7/4r3/8/8/8/8/3F4/4K3 b - - 0 1");
    Move frozenCheck = make_move(SQ_E7, SQ_E2);
    check(!pos.gives_check(frozenCheck),
          "a newly frozen direct checker still gave check");

    set_position(pos, states, "composable-check-morph-in",
                 "5k2/8/8/8/8/8/4B3/K7 w - - 0 1");
    Move checkMorphIn = make_move(SQ_E2, SQ_F3);
    check(pos.gives_check(checkMorphIn),
          "a move morph into a rook was omitted from direct check detection");
    check(MoveList<QUIET_CHECKS>(pos).contains(checkMorphIn),
          "quiet-check generation omitted a check created by move morph");

    set_position(pos, states, "composable-check-morph-out",
                 "4k3/8/8/8/8/8/4R3/4K3 w - - 0 1");
    Move checkMorphOut = make_move(SQ_E2, SQ_E3);
    check(!pos.gives_check(checkMorphOut),
          "a move morph out of a rook retained a false direct check");

    set_position(pos, states, "composable-pseudoroyal-morph-in",
                 "5rk1/8/8/8/8/8/4B3/K7 w - - 0 1");
    Move pseudoRoyalMorphIn = make_move(SQ_E2, SQ_F3);
    check(!pos.legal(pseudoRoyalMorphIn),
          "a move-morphed pseudo-royal was allowed to move into attack");

    set_position(pos, states, "composable-color-attack",
                 "4k3/8/8/8/8/4p3/4R3/4K3 w - - 0 1");
    Move colorAttackCheck = make_move(SQ_E2, SQ_E3);
    check(!pos.gives_check(colorAttackCheck),
          "a color-changing mover was still reported as checking");

    set_position(pos, states, "composable-compact-check",
                 "4k3/8/8/8/8/8/4R3/4K3 w - - 0 1");
    Move compactCheck = make_move(SQ_E2, SQ_E7);
    simulated = pos.simulated_move_info(compactCheck);
    check(!(simulated.colorOccupancy[WHITE] & square_bb(SQ_E2))
              && (simulated.colorOccupancy[WHITE] & square_bb(SQ_E7)),
          "compact simulation retained stale color occupancy");
    check(pos.gives_check(compactCheck),
          "a custom compact-simulation variant missed a direct check");
    check(MoveList<QUIET_CHECKS>(pos).contains(compactCheck),
          "a custom compact-simulation check was not generated");

    set_position(pos, states, "composable-color-promotion",
                 "7k/4P3/8/8/8/8/8/K7 w - - 0 1");
    Move colorPromotion = make<PROMOTION>(SQ_E7, SQ_E8, QUEEN);
    simulated = pos.simulated_move_info(colorPromotion);
    check(simulated.placedPiece == make_piece(BLACK, QUEEN),
          "promotion simulation did not apply the final color");
    check(!pos.gives_check(colorPromotion),
          "a color-changing promotion was still reported as checking");

    set_position(pos, states, "composable-freeze-evasion",
                 "k3r3/8/3F4/8/8/8/8/4K3 w - - 0 1");
    Move freezeEvasion = make_move(SQ_D6, SQ_D7);
    check(pos.evasion_checkers() & square_bb(SQ_E8),
          "freeze-evasion test position was not in check");
    check(pos.legal(freezeEvasion),
          "freezer move next to a checker was rejected as illegal");
    check(pos.pseudo_legal(freezeEvasion),
          "freezer evasion was rejected as pseudo-illegal");
    check(MoveList<LEGAL>(pos).contains(freezeEvasion),
          "freezer move next to a checker was not generated as an evasion");

    set_position(pos, states, "composable-trap-evasion",
                 "k2pr3/2B5/8/8/8/8/8/4K3 w - - 0 1");
    Move trapEvasion = make_move(SQ_C7, SQ_D8);
    check(pos.evasion_checkers() & square_bb(SQ_E8),
          "trap-evasion test position was not in check");
    check(pos.legal(trapEvasion) && pos.pseudo_legal(trapEvasion),
          "capturing a trap protector did not resolve check");
    check(MoveList<LEGAL>(pos).contains(trapEvasion),
          "trap-protector capture was not generated as an evasion");

    set_position(pos, states, "composable-trap-quiet-check",
                 "4k3/8/8/8/3RP3/8/8/K3R3 w - - 0 1");
    Move trapQuietCheck = make_move(SQ_D4, SQ_D5);
    check(pos.gives_check(trapQuietCheck),
          "moving a trap protector did not expose a discovered check");
    check(MoveList<QUIET_CHECKS>(pos).contains(trapQuietCheck),
          "trap-induced discovered check was not generated");

    set_position(pos, states, "composable-trap-pawn-quiet-check",
                 "4k3/8/8/8/3PB3/8/8/4R2K w - - 0 1");
    Move trapPawnQuietCheck = make_move(SQ_D4, SQ_D5);
    check(pos.gives_check(trapPawnQuietCheck),
          "moving a pawn protector did not expose a discovered check");
    check(MoveList<QUIET_CHECKS>(pos).contains(trapPawnQuietCheck),
          "trap-induced pawn discovered check was not generated");

    set_position(pos, states, "composable-royal-capture-filter",
                 "7k/8/8/8/8/8/r3p3/4K3 w - - 0 1");
    Move royalCapture = make_move(SQ_E1, SQ_E2);
    check(pos.legal(royalCapture),
          "royal capture used the captured piece type for attack filtering");

    set_position(pos, states, "composable-rex-royal-capture",
                 "7k/8/8/8/8/8/4q2q/4K3 w - - 0 1");
    Move rexRoyalCapture = make_move(SQ_E1, SQ_E2);
    check(pos.legal(rexRoyalCapture),
          "rex-exclusive royal capture used the captured type for attack filtering");

    set_position(pos, states, "composable-freeze-double-evasion",
                 "1F5k/8/2n1n3/8/3K4/8/8/8 w - - 0 1");
    Move doubleFreezeEvasion = make_move(SQ_B8, SQ_D7);
    check(popcount(pos.evasion_checkers()) == 2,
          "double-freeze-evasion test position did not have two checkers");
    check(pos.legal(doubleFreezeEvasion),
          "freezer move did not neutralize both checkers");
    check(MoveList<LEGAL>(pos).contains(doubleFreezeEvasion),
          "double-freeze evasion was not generated");

    set_position(pos, states, "composable-rex-blast",
                 "7k/8/8/8/8/8/4q3/4K3 w - - 0 1");
    Move rexCapture = make_move(SQ_E1, SQ_E2);
    simulated = pos.simulated_move_info(rexCapture);
    check(!(simulated.occupiedAfterEffects & square_bb(SQ_E2)),
          "rex-exclusive king capture was incorrectly treated as blast-immune");
    states->emplace_back();
    pos.do_move(rexCapture, states->back());
    check(pos.piece_on(SQ_E2) == NO_PIECE,
          "rex-exclusive king capture disagreed with committed blast effects");
    pos.undo_move(rexCapture);
    states->pop_back();

    set_position(pos, states, "composable-stack-morph",
                 "k3r3/8/3A4/3A4/8/8/8/4K3 w - - 0 1");
    Move stackMorph = make<STACK>(SQ_D6, SQ_D7);
    simulated = pos.simulated_move_info(stackMorph);
    check(!(simulated.freezerOccupancy[WHITE] & square_bb(SQ_D7)),
          "stack simulation retained a freezer before applying move morph");
    check(!pos.legal(stackMorph),
          "stack move was accepted using the pre-morph freezer type");

    set_position(pos, states, "composable-stack-passive",
                 "k7/8/3A4/3An3/8/8/8/4K3 w - - 0 1");
    Move stackPassive = make<STACK>(SQ_D6, SQ_D5);
    simulated = pos.simulated_move_info(stackPassive);
    check(pos.variant()->blastPassiveTypes & piece_set(QUEEN),
          "stacked passive regression did not configure its burner");
    check(simulated.type_pieces(WHITE, QUEEN) & square_bb(SQ_D5),
          "stacked passive regression did not produce its final type");
    check(pos.blast_pattern(SQ_D5) & square_bb(SQ_E5),
          "stacked passive regression did not configure an adjacent blast");
    check(simulated.removedByEffects & square_bb(SQ_E5),
          "stacked passive burner was omitted from move simulation");
    states->emplace_back();
    pos.do_move(stackPassive, states->back());
    check(pos.piece_on(SQ_E5) == NO_PIECE,
          "stacked passive burner disagreed with committed effects");
    pos.undo_move(stackPassive);
    states->pop_back();

    set_position(pos, states, "composable-unstack-morph",
                 "k3r3/8/3B4/8/8/8/8/4K3 w - - 0 1");
    Move unstackMorph = make<UNSTACK>(SQ_D6, SQ_D7);
    simulated = pos.simulated_move_info(unstackMorph);
    check(simulated.freezerOccupancy[WHITE] & square_bb(SQ_D7),
          "unstack simulation missed the post-morph freezer type");
    check(pos.legal(unstackMorph),
          "unstack move was rejected without applying its move morph");

    set_position(pos, states, "composable-unstack-passive",
                 "k7/8/2nB4/8/8/8/8/4K3 w - - 0 1");
    Move unstackPassive = make<UNSTACK>(SQ_D6, SQ_D7);
    simulated = pos.simulated_move_info(unstackPassive);
    check(pos.variant()->blastPassiveTypes & piece_set(CUSTOM_PIECE_1),
          "unstacked passive regression did not configure its burner");
    check(simulated.type_pieces(WHITE, CUSTOM_PIECE_1) & square_bb(SQ_D6),
          "unstacked passive regression did not retain its final source type");
    check(pos.blast_pattern(SQ_D6) & square_bb(SQ_C6),
          "unstacked passive regression did not configure an adjacent blast");
    check(simulated.removedByEffects & square_bb(SQ_C6),
          "unstacked passive burner was omitted from move simulation");
    states->emplace_back();
    pos.do_move(unstackPassive, states->back());
    check(pos.piece_on(SQ_C6) == NO_PIECE,
          "unstacked passive burner disagreed with committed effects");
    pos.undo_move(unstackPassive);
    states->pop_back();

    set_position(pos, states, "composable-death-freeze",
                 "k7/8/8/8/8/4r3/3pQ3/4K3 w - - 0 1");
    Move deathFreezeCapture = make_move(SQ_E2, SQ_D2);
    simulated = pos.simulated_move_info(deathFreezeCapture);
    check(!(simulated.occupiedAfterEffects & square_bb(SQ_E2))
          && (simulated.occupiedAfterEffects & square_bb(SQ_D2))
          && (simulated.addedDeadSquares & square_bb(SQ_D2)),
          "death-on-capture simulation omitted the dead-square blocker");
    {
        Position::SimulatedMoveGuard guard(pos, deathFreezeCapture);
        check(!(pos.freeze_squares(BLACK) & square_bb(SQ_E3)),
              "dead freezer continued neutralizing a checker");
    }
    check(!pos.legal(deathFreezeCapture),
          "move with a dead freezer incorrectly remained legal");

    set_position(pos, states, "composable-death-freeze",
                 "r7/8/8/8/8/p7/Q7/K7 w - - 0 1");
    Move deathDeadBlock = make_move(SQ_A2, SQ_A3);
    simulated = pos.simulated_move_info(deathDeadBlock);
    check(simulated.addedDeadSquares & square_bb(SQ_A3),
          "death-on-capture simulation omitted the new dead square");
    check(simulated.occupiedAfterEffects & square_bb(SQ_A3),
          "death-on-capture simulation omitted the dead-square blocker");
    check(pos.legal(deathDeadBlock),
          "death-on-capture dead square did not block the projected slider");
    states->emplace_back();
    pos.do_move(deathDeadBlock, states->back());
    check(pos.state()->deadSquares & square_bb(SQ_A3),
          "death-on-capture move did not install the dead square");
    pos.undo_move(deathDeadBlock);
    states->pop_back();

    set_position(pos, states, "composable-color-freeze",
                 "k7/8/8/8/8/4R3/2rpQ3/4K3 w - - 0 1");
    Move colorFreezeCapture = make_move(SQ_E2, SQ_D2);
    simulated = pos.simulated_move_info(colorFreezeCapture);
    check(simulated.colorOccupancy[BLACK] & square_bb(SQ_D2),
          "color-changing freezer remained on its original side in simulation");
    {
        Position::SimulatedMoveGuard guard(pos, colorFreezeCapture);
        check(pos.freeze_squares(WHITE) & square_bb(SQ_E3),
              "color-changing freezer did not freeze its former allies");
        check(!(pos.freeze_squares(BLACK) & square_bb(SQ_C2)),
              "color-changing freezer continued freezing its former allies");
    }
    states->emplace_back();
    pos.do_move(colorFreezeCapture, states->back());
    check(pos.piece_on(SQ_D2) == make_piece(BLACK, QUEEN),
          "committed color-changing freezer disagreed with simulation");
    pos.undo_move(colorFreezeCapture);
    states->pop_back();

    set_position(pos, states, "composable-trap-final-color",
                 "7k/8/8/8/3RP3/8/8/K7 w - - 0 1");
    Move trapColorProtector = make_move(SQ_D4, SQ_F4);
    simulated = pos.simulated_move_info(trapColorProtector);
    check(simulated.occupiedAfterEffects & square_bb(SQ_F4),
          "trap color-order simulation removed the mover");
    check(simulated.placedPiece == make_piece(BLACK, ROOK),
          "trap color-order simulation did not finalize the mover color");
    check(simulated.colorOccupancy[BLACK] & square_bb(SQ_F4),
          "trap color-order simulation did not recolor the protector");
    check(!(simulated.occupiedAfterEffects & square_bb(SQ_E4)),
          "trap occupant survived after its recolored protector moved");
    states->emplace_back();
    pos.do_move(trapColorProtector, states->back());
    check(pos.piece_on(SQ_F4) == make_piece(BLACK, ROOK)
              && pos.piece_on(SQ_E4) == NO_PIECE,
          "committed trap color ordering retained an unprotected occupant");
    pos.undo_move(trapColorProtector);
    states->pop_back();

    set_position(pos, states, "composable-flip-trap",
                 "8/8/8/8/2E1e3/8/8/8 w - - 0 1");
    Move flipTrap = make_move(SQ_C4, SQ_D4);
    simulated = pos.simulated_move_info(flipTrap, false);
    check(simulated.colorOccupancy[WHITE] & square_bb(SQ_E4),
          "flip simulation did not apply the pre-move color change before effects");
    simulated = pos.simulated_move_info(flipTrap);
    check(simulated.colorOccupancy[WHITE] & square_bb(SQ_E4),
          "flip simulation did not apply the pre-move color change: white="
              + std::to_string(bool(simulated.colorOccupancy[WHITE] & square_bb(SQ_E4)))
              + " black=" + std::to_string(bool(simulated.colorOccupancy[BLACK] & square_bb(SQ_E4))));
    check(simulated.occupiedAfterEffects & square_bb(SQ_E4),
          "trap simulation ignored a pre-move color change");
    states->emplace_back();
    pos.do_move(flipTrap, states->back());
    check(pos.piece_on(SQ_E4) == make_piece(WHITE, CUSTOM_PIECE_1),
          "committed flip/trap ordering disagreed with simulation");
    pos.undo_move(flipTrap);
    states->pop_back();

    set_position(pos, states, "composable-blast-promotion-trap-color",
                 "8/8/8/8/3er3/8/4E3/8 w - - 0 1");
    Move blastPromotionTrapColor = make_move(SQ_E2, SQ_E3);
    simulated = pos.simulated_move_info(blastPromotionTrapColor);
    check(simulated.blastPromotionOccupancy & square_bb(SQ_D4),
          "blast-promotion trap regression did not promote the bystander");
    check(simulated.occupiedAfterEffects & square_bb(SQ_E4),
          "simulation treated a blast-promoted bystander as a capture");
    states->emplace_back();
    pos.do_move(blastPromotionTrapColor, states->back());
    check(pos.piece_on(SQ_E4) == make_piece(BLACK, CUSTOM_PIECE_2),
          "committed blast-promotion trap ordering changed the mover color");
    pos.undo_move(blastPromotionTrapColor);
    states->pop_back();

    set_position(pos, states, "composable-flip-blast",
                 "8/8/8/8/3e4/8/4E3/8 w - - 0 1");
    Move flipBlast = make_move(SQ_E2, SQ_E3);
    simulated = pos.simulated_move_info(flipBlast);
    check(simulated.type_pieces(WHITE, QUEEN) & square_bb(SQ_D4),
          "flip/blast simulation retained the bystander's old color");
    states->emplace_back();
    pos.do_move(flipBlast, states->back());
    check(pos.piece_on(SQ_D4) == make_piece(WHITE, QUEEN),
          "committed flip/blast ordering disagreed with simulation");
    pos.undo_move(flipBlast);
    states->pop_back();

    set_position(pos, states, "composable-flip-surround",
                 "8/8/8/4E3/4e3/8/4E3/8 w - - 0 1");
    Move flipSurround = make_move(SQ_E2, SQ_E3);
    simulated = pos.simulated_move_info(flipSurround);
    check(!(simulated.removedByEffects & square_bb(SQ_E4)),
          "flip/surround simulation used the bystander's old color");
    states->emplace_back();
    pos.do_move(flipSurround, states->back());
    check(pos.piece_on(SQ_E4) == make_piece(WHITE, CUSTOM_PIECE_1),
          "committed flip/surround ordering disagreed with simulation");
    pos.undo_move(flipSurround);
    states->pop_back();

    set_position(pos, states, "composable-liberty-trap",
                 "9/9/9/9/3P5/2PpP4/9/p8 w - - 0 1");
    Move libertyTrap = parse_move(pos, "P@d3");
    simulated = pos.simulated_move_info(libertyTrap);
    check(simulated.removedByEffects & square_bb(SQ_D4),
          "liberty removal was omitted before simulated trap resolution");
    states->emplace_back();
    pos.do_move(libertyTrap, states->back());
    check(pos.piece_on(SQ_D4) == NO_PIECE && pos.piece_on(SQ_A1) == NO_PIECE,
          "committed liberty/trap ordering disagreed with simulation");
    pos.undo_move(libertyTrap);
    states->pop_back();

    set_position(pos, states, "composable-trap-final-death",
                 "7k/8/8/8/3RPp2/8/8/K7 w - - 0 1");
    Move trapDeathProtector = make_move(SQ_D4, SQ_F4);
    simulated = pos.simulated_move_info(trapDeathProtector);
    check(!(simulated.occupiedAfterEffects & square_bb(SQ_E4)),
          "trap occupant survived after its capturer died");
    states->emplace_back();
    pos.do_move(trapDeathProtector, states->back());
    check(pos.piece_on(SQ_E4) == NO_PIECE,
          "committed trap death ordering retained an unprotected occupant");
    pos.undo_move(trapDeathProtector);
    states->pop_back();

    set_position(pos, states, "composable-color-attack",
                 "7k/8/8/8/8/8/4R3/4K3 w - - 0 1");
    Move colorAttack = make_move(SQ_E2, SQ_E3);
    check(!pos.legal(colorAttack),
          "color-changing mover left its own king attacked in simulation");

    set_position(pos, states, "composable-potion-freeze",
                 "4r2k/8/8/8/8/8/6N1/4K3[F] w - - 0 1");
    Move potionFreezeEvasion = parse_move(pos, "f@e7,g2f4");
    check(pos.evasion_checkers() & square_bb(SQ_E8),
          "potion freeze evasion test position was not in check");
    check(potionFreezeEvasion != MOVE_NONE
          && MoveList<LEGAL>(pos).contains(potionFreezeEvasion),
          "freeze potion did not admit a move that neutralized the checker");

    set_position(pos, states, "composable-potion-freeze",
                 "4r2k/8/8/1B6/8/8/8/4K3[F] w - - 0 1");
    Move potionCaptureEvasion = parse_move(pos, "f@a1,b5e8");
    check(pos.evasion_checkers() & square_bb(SQ_E8),
          "potion capture evasion test position was not in check");
    check(MoveList<LEGAL>(pos).contains(potionCaptureEvasion),
          "freeze potion omitted an evasion that captured the checker");

    set_position(pos, states, "composable-potion-freeze",
                 "4r2k/8/8/8/8/8/8/4K3[F] w - - 0 1");
    Move potionKingEvasion = parse_move(pos, "f@a1,e1f1");
    check(pos.evasion_checkers() & square_bb(SQ_E8),
          "potion king-evasion test position was not in check");
    check(potionKingEvasion != MOVE_NONE
          && MoveList<LEGAL>(pos).contains(potionKingEvasion),
          "freeze potion rejected a safe king destination while checking the old square");

    set_position(pos, states, "composable-drop-freeze-check",
                 "4k3/8/8/8/8/8/8/3f3K[R] w - - 0 1");
    Move frozenCheckDrop = make_drop(SQ_E1, ROOK, ROOK);
    check(!pos.gives_check(frozenCheckDrop),
          "a frozen checking drop was reported as checking");
    check(!MoveList<QUIET_CHECKS>(pos).contains(frozenCheckDrop),
          "quiet-check generation retained a frozen checking drop");

    set_position(pos, states, "composable-drop-trap-check",
                 "4k3/8/8/8/8/8/8/7K[R] w - - 0 1");
    Move trappedCheckDrop = make_drop(SQ_E1, ROOK, ROOK);
    check(!pos.gives_check(trappedCheckDrop),
          "a trapped checking drop was reported as checking");
    check(!MoveList<QUIET_CHECKS>(pos).contains(trappedCheckDrop),
          "quiet-check generation retained a trapped checking drop");

    set_position(pos, states, "composable-drop-trap-discovered-check",
                 "k7/8/8/8/p7/8/7K/R7[R] w - - 0 1");
    Move trapDiscoveredCheckDrop = make_drop(SQ_H1, ROOK, ROOK);
    SimulatedMoveInfo trapDiscoveredCheckInfo = pos.simulated_move_info(trapDiscoveredCheckDrop);
    check(!(trapDiscoveredCheckInfo.occupiedAfterEffects & square_bb(SQ_A4)),
          "trap removal from a drop did not remove the line blocker in simulation");
    check(MoveList<LEGAL>(pos).contains(trapDiscoveredCheckDrop),
          "trap-discovered drop test move was not generated as legal");
    check(pos.gives_check(trapDiscoveredCheckDrop),
          "trap removal from a drop did not expose a discovered check");
    check(MoveList<QUIET_CHECKS>(pos).contains(trapDiscoveredCheckDrop),
          "quiet-check generation discarded a trap-discovered checking drop");

    set_position(pos, states, "composable-gate-trap",
                 "4k3/8/8/8/8/8/4R3/4K3 w - - 0 1");
    pos.state()->gatesBB[WHITE] |= square_bb(SQ_E2);
    Move trapMove = make_move(SQ_E1, SQ_F1);
    check(pos.legal(trapMove), "trap gate cleanup test move was not legal");
    states->emplace_back();
    pos.do_move(trapMove, states->back());
    check(pos.piece_on(SQ_E2) == NO_PIECE
          && !(pos.gates(WHITE) & square_bb(SQ_E2)),
          "trap removal left a stale gate square");
    pos.undo_move(trapMove);
    states->pop_back();

    set_position(pos, states, "composable-hopper-morph",
                 "k7/3q4/8/8/4B3/3D4/8/8 w - - 0 1");
    Move hopperMorph = make_move(SQ_E4, SQ_D4);
    simulated = pos.simulated_move_info(hopperMorph);
    check(simulated.placedPiece == make_piece(WHITE, QUEEN),
          "hopper simulation retained the pre-morph hurdle identity");
    Bitboard simulatedHopperAttackers;
    {
        Position::SimulatedMoveGuard guard(pos, hopperMorph);
        Position::SimulatedMoveInfoGuard view(pos);
        view.set(simulated);
        check(pos.piece_at(SQ_D4, simulated.occupiedAfterEffects) == make_piece(WHITE, QUEEN),
              "simulated piece view lost the post-morph hurdle");
        simulatedHopperAttackers = pos.attackers_to(
            SQ_D6, simulated.occupiedAfterEffects, WHITE,
            pos.pieces(JANGGI_CANNON), &simulated);
    }
    check(simulatedHopperAttackers & square_bb(SQ_D3),
          "simulated hopper did not classify the post-morph hurdle");
    states->emplace_back();
    pos.do_move(hopperMorph, states->back());
    check(pos.attackers_to(SQ_D6, pos.pieces(), WHITE) & square_bb(SQ_D3),
          "hopper simulation disagreed with the committed attack map");
    pos.undo_move(hopperMorph);
    states->pop_back();

    set_position(pos, states, "composable-blast-janggi-screen",
                 "7k/8/4c3/3R4/4c3/8/8/4K3 w - - 0 1");
    Move blastCannonScreen = make_move(SQ_D5, SQ_D4);
    simulated = pos.simulated_move_info(blastCannonScreen);
    check(!(simulated.type_pieces(BLACK, JANGGI_CANNON) & square_bb(SQ_E4)),
          "blast cannon simulation retained a promoted screen as a cannon");
    Bitboard simulatedCannons = simulated.type_pieces(WHITE, JANGGI_CANNON)
                              | simulated.type_pieces(BLACK, JANGGI_CANNON);
    check(pos.attackers_to(SQ_E1, simulated.occupiedAfterEffects, BLACK,
                           pos.pieces(JANGGI_CANNON), &simulated)
              == pos.attackers_to(SQ_E1, simulated.occupiedAfterEffects, BLACK,
                                  simulatedCannons, &simulated),
          "simulated cannon attack ignored the final cannon classification");
    check(!pos.legal(blastCannonScreen),
          "blast promotion left a newly non-cannon screen undetected");

    set_position(pos, states, "composable-trap-ep-check",
                 "8/8/8/RPp4k/8/8/8/4K3 w - c6 0 1");
    Move trapEnPassant = parse_move(pos, "b5c6");
    check(!pos.gives_check(trapEnPassant),
          "en-passant check detection ignored the final trap removal");
    check(pos.legal(trapEnPassant),
          "en-passant was rejected after its discovered checker was trapped");

    set_position(pos, states, "composable-trap-ep-legality",
                 "7k/8/8/1Ppr4/8/8/8/3K4 w - c6 0 1");
    Move trapEvasionEnPassant = make<EN_PASSANT>(SQ_B5, SQ_C6);
    check(pos.evasion_checkers() & square_bb(SQ_D5),
          "trap en-passant evasion test position was not in check");
    check(pos.legal(trapEvasionEnPassant),
          "en-passant did not use trap removal when resolving check");

    set_position(pos, states, "composable-trap-ep-blocker",
                 "r6k/8/8/RPp5/8/8/8/K7 w - c6 0 1");
    Move trapBlockerEnPassant = make<EN_PASSANT>(SQ_B5, SQ_C6);
    check(!pos.legal(trapBlockerEnPassant),
          "en-passant ignored trap removal of a king-line blocker");

    set_position(pos, states, "composable-castle-freeze-destination",
                 "k7/8/8/8/8/8/4n3/4K2R w K - 0 1");
    check(pos.legal(make<CASTLING>(SQ_E1, SQ_H1)),
          "castling destination was checked before its final freezer arrived");

    set_position(pos, states, "composable-castle-effects",
                 "7k/8/8/8/8/8/6r1/4K2R w K - 0 1");
    Move trapCastle = make<CASTLING>(SQ_E1, SQ_H1);
    simulated = pos.simulated_move_info(trapCastle);
    check(!(simulated.occupiedAfterEffects & square_bb(SQ_G2)),
          "castling simulation retained an unprotected trap attacker");
    check(simulated.occupiedAfterEffects & square_bb(SQ_G1),
          "castling simulation did not place the king on its destination");
    check(!(pos.attackers_to_king(SQ_G1, simulated.occupiedAfterEffects, BLACK,
                                  pos.pieces(JANGGI_CANNON), NO_PIECE_TYPE, &simulated)
            & simulated.occupiedAfterEffects),
          "post-effect castling attack map still sees an attacker");
    check(pos.legal(trapCastle),
          "castling destination retained an attacker removed by a trap");

    set_position(pos, states, "composable-royal-path-freeze",
                 "8/8/8/8/3q4/8/8/4K3 w - - 0 1");
    check(pos.attackers_to(SQ_E3, pos.pieces(), BLACK,
                           pos.pieces(JANGGI_CANNON)) & square_bb(SQ_D4),
          "royal transit test position was not attacked on its intermediate square");
    check(!pos.legal(make_move(SQ_E1, SQ_E4)),
          "royal transit check incorrectly used the final freezer state");

    set_position(pos, states, "composable-freeze-traps-blast",
                 "8/8/8/4e3/8/8/4E3/8 w - - 0 1");
    Move blastMoverMove = make_move(SQ_E2, SQ_E3);
    simulated = pos.simulated_move_info(blastMoverMove);
    check(simulated.occupiedAfterEffects & square_bb(SQ_E3),
          "blast-promoted mover disappeared from move simulation");
    states->emplace_back();
    pos.do_move(blastMoverMove, states->back());
    check(type_of(pos.piece_on(SQ_E3)) == QUEEN,
          "blast-promoted mover had the wrong committed type");
    pos.undo_move(blastMoverMove);
    states->pop_back();

    set_position(pos, states, "composable-freeze-traps-blast",
                 "8/8/8/8/8/8/4E3/4e3 w - - 0 1");
    Move blastCapture = make_move(SQ_E2, SQ_E3);
    simulated = pos.simulated_move_info(blastCapture);
    check(simulated.occupiedAfterEffects & square_bb(SQ_E3),
          "blast-promoted capture destination disappeared from simulation");
    states->emplace_back();
    pos.do_move(blastCapture, states->back());
    check(type_of(pos.piece_on(SQ_E3)) == QUEEN,
          "blast-promoted capture destination had the wrong committed type");
    pos.undo_move(blastCapture);
    states->pop_back();

    set_position(pos, states, "composable-blast-promotion-color",
                 "8/8/8/4e3/8/8/4E3/8 w - - 0 1");
    Move blastPromotionColor = make_move(SQ_E2, SQ_E3);
    simulated = pos.simulated_move_info(blastPromotionColor);
    check(simulated.blastPromotionOccupancy & square_bb(SQ_E3),
          "blast-promotion color regression did not promote its survivor");
    check(!(simulated.removedByEffects & square_bb(SQ_E3)),
          "blast-promotion color regression removed its survivor in simulation");
    check(simulated.colorOccupancy[WHITE] & square_bb(SQ_E3),
          "blast-promotion color simulation lost the mover color");
    check(!(simulated.colorOccupancy[BLACK] & square_bb(SQ_E3)),
          "blast-promotion survivor incorrectly triggered simulated color change");
    states->emplace_back();
    pos.do_move(blastPromotionColor, states->back());
    check(pos.piece_on(SQ_E3) == make_piece(WHITE, QUEEN),
          "blast-promotion survivor incorrectly triggered committed color change");
    pos.undo_move(blastPromotionColor);
    states->pop_back();

    set_position(pos, states, "composable-blast-promotion-trap-color-mismatch",
                 "8/8/8/8/4E3/8/4E3/8 w - - 0 1");
    Move blastPromotionTrapColorMismatch = make_move(SQ_E2, SQ_E3);
    simulated = pos.simulated_move_info(blastPromotionTrapColorMismatch);
    check(!(simulated.occupiedAfterEffects & square_bb(SQ_E4)),
          "blast-promotion color simulation did not remove the unprotected trap piece");
    states->emplace_back();
    pos.do_move(blastPromotionTrapColorMismatch, states->back());
    check(pos.piece_on(SQ_E3) == make_piece(BLACK, QUEEN),
          "committed blast-promotion color ordering lost the promoted mover");
    check(pos.piece_on(SQ_E4) == NO_PIECE,
          "committed blast-promotion color ordering disagreed with trap simulation");
    pos.undo_move(blastPromotionTrapColorMismatch);
    states->pop_back();

    set_position(pos, states, "composable-blast-immune",
                 "8/8/8/4e3/8/8/4R3/8 w - - 0 1");
    Move blastImmuneMove = make_move(SQ_E2, SQ_E3);
    simulated = pos.simulated_move_info(blastImmuneMove);
    check(simulated.blastImmuneOccupancy & square_bb(SQ_E3),
          "moved blast-immune piece kept its immunity on the old square");
    check(simulated.occupiedAfterEffects & square_bb(SQ_E3),
          "moved blast-immune piece was removed from move simulation");
    states->emplace_back();
    pos.do_move(blastImmuneMove, states->back());
    check(pos.piece_on(SQ_E3) != NO_PIECE,
          "moved blast-immune piece was removed by committed effects");
    pos.undo_move(blastImmuneMove);
    states->pop_back();

    set_position(pos, states, "composable-rifle-morph",
                 "4k3/8/8/8/8/4p3/4R3/4K3 w - - 0 1");
    Move rifleMorph = parse_move(pos, "e2e3");
    check(pos.rifle_capture(rifleMorph) && pos.capture(rifleMorph),
          "rifle morph regression did not create a rifle capture");
    simulated = pos.simulated_move_info(rifleMorph);
    check(simulated.blastImmuneOccupancy & square_bb(SQ_E2),
          "rifle morph did not update the shooter's blast immunity");
    check(simulated.occupiedAfterEffects & square_bb(SQ_E2),
          "rifle morph shooter disappeared from move simulation");
    states->emplace_back();
    pos.do_move(rifleMorph, states->back());
    check(type_of(pos.piece_on(SQ_E2)) == QUEEN,
          "rifle shooter did not receive its configured morph");
    pos.undo_move(rifleMorph);
    states->pop_back();

    set_position(pos, states, "composable-rifle-morph-out",
                 "4k3/8/8/8/8/4p3/4Q3/4K3 w - - 0 1");
    Move rifleMorphOut = parse_move(pos, "e2e3");
    simulated = pos.simulated_move_info(rifleMorphOut);
    check(!(simulated.blastImmuneOccupancy & square_bb(SQ_E2))
          && !(simulated.occupiedAfterEffects & square_bb(SQ_E2)),
          "rifle morph out retained blast immunity in simulation");
    states->emplace_back();
    pos.do_move(rifleMorphOut, states->back());
    check(pos.piece_on(SQ_E2) == NO_PIECE,
          "rifle morph out retained blast immunity in committed effects");
    pos.undo_move(rifleMorphOut);
    states->pop_back();

    set_position(pos, states, "rifle-key-audit",
                 "4k2r/8/8/8/8/8/8/4K2R w KQkq - 0 1");
    Move rifleRook = parse_move(pos, "h1h8");
    states->emplace_back();
    pos.do_move(rifleRook, states->back());
    check(pos.piece_on(SQ_H1) == make_piece(WHITE, ROOK)
          && pos.piece_on(SQ_H8) == NO_PIECE
          && pos.can_castle(WHITE_OO)
          && !pos.can_castle(BLACK_OO),
          "rifle capture changed castling rights for the stationary shooter");
    Position rifleRookReloaded;
    StateListPtr rifleRookStates;
    set_position(rifleRookReloaded, rifleRookStates, "rifle-key-audit", pos.fen().c_str());
    check(pos.key() == rifleRookReloaded.key(),
          "rifle capture produced a key that did not match its FEN");
    pos.undo_move(rifleRook);
    states->pop_back();

    set_position(pos, states, "rifle-key-audit",
                 "4k3/8/8/8/8/3p4/4P3/4K3 w - - 0 1");
    Move riflePawn = parse_move(pos, "e2d3");
    states->emplace_back();
    pos.do_move(riflePawn, states->back());
    Position riflePawnReloaded;
    StateListPtr riflePawnStates;
    set_position(riflePawnReloaded, riflePawnStates, "rifle-key-audit", pos.fen().c_str());
    check(pos.pawn_key() == riflePawnReloaded.pawn_key()
          && pos.key() == riflePawnReloaded.key(),
          "rifle pawn capture produced a key that did not match its FEN");
    pos.undo_move(riflePawn);
    states->pop_back();

    set_position(pos, states, "composable-ep-ghost",
                 "4k3/8/3r4/3pP3/8/8/8/4K3[F] w - d6 0 1");
    Move occupiedEp = make<EN_PASSANT>(SQ_E5, SQ_D6);
    simulated = pos.simulated_move_info(occupiedEp);
    check(!(simulated.colorOccupancy[BLACK] & square_bb(SQ_D6))
          && (simulated.colorOccupancy[WHITE] & square_bb(SQ_D6)),
          "occupied en-passant target left a ghost enemy in simulation");

    set_position(pos, states, "composable-trap-swap",
                 "4k3/8/8/8/8/8/8/4Kr2 w - - 0 1");
    check(!pos.legal(make<SWAP>(SQ_E1, SQ_F1)),
          "a swap accepted after a trap removed the moving royal");

    set_position(pos, states, "composable-blast-promotion-check",
                 "7k/8/8/8/4e2K/8/4E3/8 w - - 0 1");
    Move blastPromotionCheck = make_move(SQ_E2, SQ_E3);
    check(!pos.legal(blastPromotionCheck),
          "a blast-promoted bystander was omitted from post-move king safety");

    set_position(pos, states, "composable-wrap-trap",
                 "1r1R4 w - - 0 1");
    Move wrappedProtection = make_move(SQ_D1, SQ_E1);
    simulated = pos.simulated_move_info(wrappedProtection);
    check(simulated.removedByEffects & square_bb(SQ_B1),
          "one-rank wrapping treated a trap occupant as its own protector");

    set_position(pos, states, "composable-gated-pawn-blast",
                 "7k/8/8/8/8/8/4R3/7K w - - 0 1");
    Move gatedPawnBlast = make_gating<NORMAL>(SQ_E2, SQ_E3, PAWN, SQ_E4);
    simulated = pos.simulated_move_info(gatedPawnBlast);
    check(simulated.occupiedAfterEffects & square_bb(SQ_E4),
          "gated pawn was removed using its pre-move classification");

    set_position(pos, states, "composable-demotion-morph",
                 "4k3/8/8/8/8/8/8/2B~:P1K3 w - - 0 1");
    Move demotionMorph = make<PIECE_DEMOTION>(SQ_C1, SQ_D1);
    simulated = pos.simulated_move_info(demotionMorph);
    check(simulated.freezerOccupancy[WHITE] & square_bb(SQ_D1),
          "demotion skipped the subsequent move morph in simulation");
    states->emplace_back();
    pos.do_move(demotionMorph, states->back());
    check(type_of(pos.piece_on(SQ_D1)) == QUEEN,
          "demotion did not receive its configured move morph");
    pos.undo_move(demotionMorph);
    states->pop_back();

    set_position(pos, states, "composable-blast-surround",
                 "8/8/8/4R3/4e3/8/4E3/8 w - - 0 1");
    Move blastSurround = make_move(SQ_E2, SQ_E3);
    simulated = pos.simulated_move_info(blastSurround);
    check(simulated.blastPromotionOccupancy & square_bb(SQ_E4),
          "blast promotion candidate was lost to surround removal");
    check(!(simulated.removedByEffects & square_bb(SQ_E4)),
          "blast promotion candidate was removed after promotion simulation");
    check(simulated.occupiedAfterEffects & square_bb(SQ_E4),
          "blast promotion disagreed with surround removal in simulation");
    states->emplace_back();
    pos.do_move(blastSurround, states->back());
    check(type_of(pos.piece_on(SQ_E4)) == QUEEN,
          "blast promotion disagreed with surround removal in do_move");
    pos.undo_move(blastSurround);
    states->pop_back();

    set_position(pos, states, "composable-print-overlap",
                 "4k3/8/8/3Rr3/8/8/8/4K3[JJFFFFjjffff] w - - 0 1 wj:e4");
    std::ostringstream printed;
    printed << pos;
    check(!printed.str().empty(),
          "printing an overlapping generic freeze and jump zone failed");

    set_position(pos, states, "composable-trap-royal",
                 "4k3/8/8/8/8/8/8/4K2R w K - 0 1");
    check(!pos.legal(make<CASTLING>(SQ_E1, SQ_H1)),
          "castling onto an unprotected trap square removed the royal");
}

#ifdef ENABLE_COMPOUND_TURNS
void arimaa_setup() {
    Position pos;
    StateListPtr states;
    set_position(pos, states, "arimaa",
                 "8/8/8/8/8/8/8/8[RRRRRRRRCCDDHHMErrrrrrrrccddhhme] w - - 0 1");

    const std::vector<const char*> placements = {
      "R@a1", "0000", "R@b1", "0000", "R@c1", "0000", "R@d1", "0000",
      "R@e1", "0000", "R@f1", "0000", "R@g1", "0000", "R@h1", "0000",
      "C@a2", "0000", "C@b2", "0000", "D@c2", "0000", "D@d2", "0000",
      "H@e2", "0000", "H@f2", "0000", "M@g2", "0000", "E@h2",
      "R@a8", "0000", "R@b8", "0000", "R@c8", "0000", "R@d8", "0000",
      "R@e8", "0000", "R@f8", "0000", "R@g8", "0000", "R@h8", "0000",
      "C@a7", "0000", "C@b7", "0000", "D@c7", "0000", "D@d7", "0000",
      "H@e7", "0000", "H@f7", "0000", "M@g7", "0000", "E@h7"
    };
    std::vector<Move> moves;
    for (const char* notation : placements)
    {
        Move move = parse_move(pos, notation);
        moves.push_back(move);
        states->emplace_back();
        pos.do_move(move, states->back());
    }

    check(pos.game_ply() == 0, "Arimaa setup placements advanced gamePly");
    check(pos.rule50_count() == 0, "Arimaa setup placements advanced rule50");
    check(pos.side_to_move() == WHITE, "completed Arimaa setup did not start Gold");

    for (auto it = moves.rbegin(); it != moves.rend(); ++it)
    {
        pos.undo_move(*it);
        states->pop_back();
    }
    check(pos.game_ply() == 0, "undoing Arimaa setup changed gamePly");
}

void arimaa_architecture() {
    Position pos;
    StateListPtr states;
    Value result;

    set_position(pos, states, "arimaa",
                 "8/7r/8/8/8/8/R7/8 w - - 0 1");
    check(pos.at_complete_turn_boundary(),
          "a freshly loaded Arimaa position was not at a complete-turn boundary");
    Move step = parse_move(pos, "a2a3");
    states->emplace_back();
    pos.do_move(step, states->back());
    check(!pos.at_complete_turn_boundary(),
          "an Arimaa partial step was exposed as a complete-turn boundary");
    check(pos.fen().empty(),
          "a partial Arimaa step was silently serialized as complete-turn FEN");
    check(!pos.nnue_applicable(),
          "NNUE remained applicable inside an Arimaa partial turn");
    pos.undo_move(step);
    states->pop_back();
    check(pos.at_complete_turn_boundary(),
          "undoing an Arimaa partial step did not restore the turn boundary");

    set_position(pos, states, "arimaa",
                 "7r/R7/8/8/8/8/8/8 w - - 0 1");
    Move goalStep = parse_move(pos, "a7a8");
    states->emplace_back();
    pos.do_move(goalStep, states->back());
    check(!pos.is_game_end(result),
          "an Arimaa rabbit goal was adjudicated before the turn boundary");
    pos.undo_move(goalStep);
    states->pop_back();

    set_position(pos, states, "arimaa",
                 "7r/8/8/8/8/8/2R5/8 w - - 0 1");
    Move trapStep = parse_move(pos, "c2c3");
    states->emplace_back();
    pos.do_move(trapStep, states->back());
    check(!pos.is_game_end(result),
          "Arimaa rabbit extinction was adjudicated before the turn boundary");
    pos.undo_move(trapStep);
    states->pop_back();

    set_position(pos, states, "arimaa",
                 "7r/8/8/3r4/3E4/8/8/R7 w - - 0 1");
    Move ordinaryCapture = make_move(SQ_D4, SQ_D5);
    Move encodedPush = make_encoded_push(SQ_D4, SQ_D5, SQ_D6);
    check(!pos.pseudo_legal(ordinaryCapture),
          "Arimaa move-only Betza allowed an ordinary occupied-destination capture");
    check(encoded_push_square(encodedPush) == SQ_D6,
          "Arimaa encoded push did not round-trip its destination square");
    check(pos.encoded_push_legal(encodedPush),
          "Arimaa encoded push was rejected while ordinary captures were disabled");

    set_position(pos, states, "arimaa-board-size-audit",
                 "9r/10/10/10/10/10/10/10/10/R9 w - - 0 1");
    check(pos.max_file() == FILE_J && pos.max_rank() == RANK_10,
          "Arimaa rejected a configured board larger than 8x8");

    set_position(pos, states, "arimaa-wrapped-push-audit",
                 "8/8/8/r6E/8/8/8/8 w - - 0 1");
    check(pos.topology_wraps(), "cylindrical Arimaa audit did not enable wrapping");
    check(pos.attacks_from(WHITE, WAZIR, SQ_H4, Bitboard(0)) & SQ_A4,
          "wrapped Wazir adjacency missed h4-a4");
    check(pos.attacks_from(WHITE, WAZIR, SQ_A4, Bitboard(0)) & SQ_B4,
          "wrapped Wazir adjacency missed a4-b4");

    set_position(pos, states, "arimaa",
                 "R7/7r/8/8/8/8/8/8 b - - 0 1");
    check(pos.is_game_end(result, 3) && result == mated_in(3),
          "Arimaa terminal adjudication did not preserve the supplied search ply");

    set_position(pos, states, "arimaa-nonsequential-audit",
                 "8/8/8/8/8/8/8/8[RRRRRRRRCCDDHHMErrrrrrrrccddhhme] w - - 0 1");
    Move ordinaryDrop = parse_move(pos, "R@a1");
    states->emplace_back();
    pos.do_move(ordinaryDrop, states->back());
    check(pos.side_to_move() == BLACK && pos.game_ply() == 1 && pos.rule50_count() == 0,
          "sequentialSetup=false did not use ordinary drop-side transitions (side="
          + std::to_string(int(pos.side_to_move()))
          + ", ply=" + std::to_string(pos.game_ply())
          + ", rule50=" + std::to_string(pos.rule50_count()) + ")");
    pos.undo_move(ordinaryDrop);
    states->pop_back();

    set_position(pos, states, "arimaa-custom-role-audit",
                 "8/8/8/8/8/8/8/8 w - - 0 1");
    pos.put_piece(make_piece(WHITE, CUSTOM_PIECE_2), SQ_D4);
    pos.put_piece(make_piece(BLACK, CUSTOM_PIECE_1), SQ_D5);
    std::string forwardFen = pos.fen();
    set_position(pos, states, "arimaa-custom-role-audit", forwardFen.c_str());
    check(pos.flag_piece(WHITE) == CUSTOM_PIECE_2,
          "Arimaa flag role did not use the configured piece type: got "
          + std::to_string(int(pos.flag_piece(WHITE)))
          + " expected " + std::to_string(int(CUSTOM_PIECE_2)));
    Move forwardPush = make_encoded_push(SQ_D4, SQ_D5, SQ_D6);
    check(pos.encoded_push_legal(forwardPush),
          "configured rabbit Betza rejected a forward push (role="
          + std::to_string(int(pos.flag_piece(WHITE)))
          + ", source=" + std::to_string(int(type_of(pos.piece_on(SQ_D4))))
          + ", moves=" + std::to_string(bool(PseudoMoves[0][WHITE][pos.flag_piece(WHITE)][SQ_D4] & SQ_D5))
          + ")");
    set_position(pos, states, "arimaa-custom-role-audit",
                 "8/8/8/8/8/8/8/8 w - - 0 1");
    pos.put_piece(make_piece(WHITE, CUSTOM_PIECE_2), SQ_D4);
    pos.put_piece(make_piece(BLACK, CUSTOM_PIECE_1), SQ_D3);
    std::string backwardFen = pos.fen();
    set_position(pos, states, "arimaa-custom-role-audit", backwardFen.c_str());
    Move backwardPush = make_encoded_push(SQ_D4, SQ_D3, SQ_D2);
    check(!pos.encoded_push_legal(backwardPush),
          "configured rabbit Betza allowed a backward push");

    set_position(pos, states, "arimaa-pusher-movement-audit",
                 "8/8/8/8/8/8/8/8 w - - 0 1");
    pos.put_piece(make_piece(WHITE, CUSTOM_PIECE_2), SQ_D4);
    pos.put_piece(make_piece(BLACK, CUSTOM_PIECE_1), SQ_D3);
    std::string pusherMovementFen = pos.fen();
    set_position(pos, states, "arimaa-pusher-movement-audit", pusherMovementFen.c_str());
    Move pusherBackward = make_encoded_push(SQ_D4, SQ_D3, SQ_D2);
    check(!pos.encoded_push_legal(pusherBackward),
          "encoded push legality depended on the configured flag piece instead of the pusher Betza");

    set_position(pos, states, "arimaa-push-rule-none-audit",
                 "8/8/8/3r4/3R4/8/8/8 w - - 0 1");
    check(!pos.has_pushing(),
          "pushPullRule=none still exposed configured pushing capability");
    check(!pos.encoded_push_legal(forwardPush),
          "pushPullRule=none accepted an Arimaa push encoding");

    set_position(pos, states, "arimaa-push-rule-generic-audit",
                 "8/8/8/3r4/3R4/8/8/8 w - - 0 1");
    check(pos.has_pushing(),
          "pushPullRule=generic hid configured pushing capability");
    check(!pos.encoded_push_legal(forwardPush),
          "pushPullRule=generic accepted an Arimaa push encoding");

    set_position(pos, states, "arimaa-push-rule-generic-audit",
                 "7r/8/8/3r4/3E4/8/8/R7 w - - 0 1");
    Move genericPull = parse_move(pos, "d4e4,d5");
    check(is_two_step_move(genericPull),
          "generic pull was not represented as a two-step move");
    states->emplace_back();
    pos.do_move(genericPull, states->back());
    check(pos.compound_turn_step() == 2,
          "generic pull advanced the compound turn by one step instead of two");
    check(!pos.at_complete_turn_boundary(),
          "generic pull incorrectly completed the compound turn");
    pos.undo_move(genericPull);
    states->pop_back();
}

void compound_turn_rules() {
    Position pos;
    StateListPtr states;

    // 1. Generic sequential setup audit
    set_position(pos, states, "generic-sequential-setup-audit",
                 "8/8/8/8/8/8/8/8[RRRRrrrr] w - - 0 1");
    std::vector<Move> setupMoves;
    auto play_setup = [&](const char* notation) {
        Move move = parse_move(pos, notation);
        states->emplace_back();
        pos.do_move(move, states->back());
        setupMoves.push_back(move);
    };
    play_setup("R@a1");
    check(pos.side_to_move() == BLACK, "generic sequential setup did not force a pass after White's drop");
    const std::string setupHandoffFen = pos.fen();
    check(setupHandoffFen.find(" setup=") == std::string::npos,
          "sequential setup handoff FEN exposed internal placement state");
    set_position(pos, states, "generic-sequential-setup-audit", setupHandoffFen.c_str());
    check(pos.side_to_move() == BLACK,
          "sequential setup FEN did not preserve the side to move");
    MoveList<LEGAL> setupHandoffMoves(pos);
    check(setupHandoffMoves.size() == 1 && is_pass(setupHandoffMoves.begin()->move),
          "sequential setup handoff FEN did not restore the forced pass");
    set_position(pos, states, "generic-sequential-setup-audit",
                 "8/8/8/8/8/8/8/8[RRRRrrrr] w - - 0 1");
    setupMoves.clear();
    play_setup("R@a1");
    play_setup("0000");
    check(pos.side_to_move() == WHITE, "generic sequential setup pass did not return to White");
    check(pos.game_ply() == 0, "generic sequential setup drop advanced gamePly");
    play_setup("R@b1");
    play_setup("0000");
    play_setup("R@c1");
    play_setup("0000");
    play_setup("R@d1");
    check(pos.side_to_move() == BLACK, "emptying White pocket did not switch sideToMove to Black");
    play_setup("R@e8");
    play_setup("0000");
    play_setup("R@f8");
    play_setup("0000");
    play_setup("R@g8");
    play_setup("0000");
    play_setup("R@h8");
    check(pos.side_to_move() == WHITE, "emptying Black pocket did not restore sideToMove to White");
    check(pos.game_ply() == 0, "generic sequential setup drops advanced gamePly before all setup complete");

    // Undoing sequential drops
    for (auto it = setupMoves.rbegin(); it != setupMoves.rend(); ++it)
    {
        pos.undo_move(*it);
        states->pop_back();
    }
    check(pos.side_to_move() == WHITE && pos.game_ply() == 0, "undoing all sequential drops failed");

    // 2. Generic compound turn generation, step costs, and repetition
    set_position(pos, states, "generic-compound-turn-audit",
                 "8/8/8/3r4/3C4/8/8/8 w - - 0 1");
    check(!variants.get("generic-compound-turn-audit")->completeTurnRepetitionIllegal,
          "generic compound turns inherited Arimaa repetition policy");
    check(variants.get("arimaa")->completeTurnRepetitionIllegal,
          "Arimaa did not enable complete-turn repetition illegality");
    check(pos.compound_turn_active(), "generic compound turn is not active");
    check(pos.compound_turn_steps() == 3, "generic compound turn steps != 3");
    check(pos.at_complete_turn_boundary(), "fresh position is not at complete turn boundary");

    // Test compound moves generation
    std::vector<CompoundMove> generated = generate_compound_moves(pos);
    check(!generated.empty(), "generate_compound_moves returned empty list");

    // Two-step action consumes 2 steps in budget of 3
    Move twoStepPush = make_encoded_push(SQ_D4, SQ_D5, SQ_D6);
    check(pos.encoded_push_legal(twoStepPush), "two-step push should be legal");
    check(pos.compound_turn_step_cost(twoStepPush) == 2, "two-step push cost != 2");

    // Parse compound move
    CompoundMove parsedTurn;
    bool ok = parse_compound_move(pos, "d4d5,d6", parsedTurn);
    check(ok && parsedTurn.length == 1, "failed to parse 1-component two-step compound move");

    // Test do/undo compound move
    alignas(Eval::NNUE::CacheLineSize) StateInfo cstates[CompoundMove::MAX_STEPS + 1];
    do_compound_move(pos, parsedTurn, cstates);
    check(pos.side_to_move() == BLACK, "do_compound_move did not switch side to move to Black");
    check(pos.at_complete_turn_boundary(), "after do_compound_move not at turn boundary");
    undo_compound_move(pos, parsedTurn);
    check(pos.side_to_move() == WHITE, "undo_compound_move did not restore side to move");
    check(pos.at_complete_turn_boundary(), "after undo_compound_move not at turn boundary");

    // A two-step action that exactly fills a two-step turn still advances and
    // undoes one logical game ply, not two internal cost units.
    set_position(pos, states, "generic-compound-turn-two-audit",
                 "8/8/8/3r4/3C4/8/8/8 w - - 0 1");
    Move exactTwoStepPush = make_encoded_push(SQ_D4, SQ_D5, SQ_D6);
    const Key exactPushKey = pos.key();
    const std::string exactPushFen = pos.fen();
    states->emplace_back();
    pos.do_move(exactTwoStepPush, states->back());
    check(pos.game_ply() == 1 && pos.side_to_move() == BLACK,
          "turnSteps=2 two-step push did not advance one logical ply");
    pos.undo_move(exactTwoStepPush);
    states->pop_back();
    check(pos.game_ply() == 0 && pos.key() == exactPushKey && pos.fen() == exactPushFen,
          "turnSteps=2 two-step push undo corrupted the logical position");

    set_position(pos, states, "arimaa-pull-turn-two-audit",
                 "7r/8/8/3r4/3E4/8/8/R7 w - - 0 1");
    Move exactTwoStepPull = make_pull(SQ_D4, SQ_E4, SQ_D5);
    check(pos.legal(exactTwoStepPull),
          "turnSteps=2 two-step pull was not legal at the turn boundary");
    const Key exactPullKey = pos.key();
    const std::string exactPullFen = pos.fen();
    states->emplace_back();
    pos.do_move(exactTwoStepPull, states->back());
    check(pos.game_ply() == 1 && pos.side_to_move() == BLACK,
          "turnSteps=2 two-step pull did not advance one logical ply");
    pos.undo_move(exactTwoStepPull);
    states->pop_back();
    check(pos.game_ply() == 0 && pos.key() == exactPullKey && pos.fen() == exactPullFen,
          "turnSteps=2 two-step pull undo corrupted the logical position");

    // A two-step pull must not fit after three ordinary component steps of a
    // four-cost turn, even through Position::legal().
    set_position(pos, states, "arimaa-push-rule-generic-audit",
                 "7r/8/8/3r4/3E4/8/8/R7 w - - 0 1");
    const char* fillerMoves[] = {"a1a2", "a2a3", "a3a4"};
    Move fillerMovesParsed[3];
    for (int i = 0; i < 3; ++i)
    {
        Move filler = parse_move(pos, fillerMoves[i]);
        fillerMovesParsed[i] = filler;
        states->emplace_back();
        pos.do_move(filler, states->back());
    }
    Move latePull = make_pull(SQ_D4, SQ_E4, SQ_D5);
    check(pos.compound_turn_step() == 3 && !pos.legal(latePull),
          "Position::legal accepted a two-step pull over the remaining turn budget");
    for (int i = 0; i < 3; ++i)
    {
        pos.undo_move(fillerMovesParsed[2 - i]);
        states->pop_back();
    }

    set_position(pos, states, "generic-compound-optional-boundary-audit",
                 "8/8/8/3r4/3C4/8/8/8 w - - 1 1");
    Move optionalBoundaryStep = parse_move(pos, "d4d3");
    states->emplace_back();
    pos.do_move(optionalBoundaryStep, states->back());
    Value optionalResult = VALUE_NONE;
    check(pos.compound_turn_step() != 0 && !pos.is_optional_game_end(optionalResult),
          "optional game-end rule fired inside a compound turn");
    pos.undo_move(optionalBoundaryStep);
    states->pop_back();

    // Test formatting compound move
    std::string formatted = compound_move_to_string(pos, parsedTurn);
    check(formatted == "d4d5,d6", "formatted compound move mismatch: " + formatted);

    // A configured pass is a complete compound turn, not a no-op component.
    set_position(pos, states, "generic-compound-pass-audit",
                 "8/8/8/3r4/3C4/8/8/8 w - - 0 1");
    std::vector<CompoundMove> passGenerated = generate_compound_moves(pos);
    check(std::any_of(passGenerated.begin(), passGenerated.end(),
                      [](const CompoundMove& move) {
                          return move.length == 1 && is_pass(move.steps[0]);
                      }),
          "pass=true compound variant did not generate a pass turn");
    for (const CompoundMove& generatedTurn : passGenerated)
    {
        for (int i = 1; i < generatedTurn.length; ++i)
            check(!is_pass(generatedTurn.steps[i]),
                  "compound generation appended a pass after an earlier step");

        const std::string text = compound_move_to_string(pos, generatedTurn);
        CompoundMove reparsed;
        check(parse_compound_move(pos, text, reparsed) && reparsed == generatedTurn,
              "generated pass-enabled compound turn did not round-trip: " + text);
    }
    CompoundMove parsedPass;
    check(parse_compound_move(pos, "0000", parsedPass)
          && parsedPass.length == 1 && is_pass(parsedPass.steps[0]),
          "pass=true compound variant did not parse a pass turn");
    do_compound_move(pos, parsedPass, cstates);
    check(pos.side_to_move() == BLACK && pos.at_complete_turn_boundary(),
          "compound pass did not complete the turn");
    undo_compound_move(pos, parsedPass);
    check(pos.side_to_move() == WHITE && pos.at_complete_turn_boundary(),
          "compound pass undo did not restore the position");

    set_position(pos, states, "generic-compound-stalemate-pass-audit",
                 "8/8/8/3r4/8/8/8/8 w - - 0 1");
    check(pos.pass(WHITE),
          "compound passOnStalemate was suppressed when ordinary passing was disabled");
    MoveList<LEGAL> stalemateMoves(pos);
    check(stalemateMoves.size() == 1 && is_pass(stalemateMoves.begin()->move),
          "compound passOnStalemate did not generate the stalemate pass");

    // Test intermediate reversal followed by real component
    set_position(pos, states, "generic-compound-pass-audit",
                 "8/8/8/3r4/3C4/8/8/8 w - - 0 1");
    CompoundMove reversalTurn;
    ok = parse_compound_move(pos, "d4e4,e4d4,d4d3", reversalTurn);
    check(ok && reversalTurn.length == 3, "failed to parse reversal followed by real component");

    // A pure reversal d4e4, e4d4 is pass-equivalent and remains filtered even
    // in the pass-enabled profile; the explicit 0000 move is canonical.
    CompoundMove pureReversal;
    ok = parse_compound_move(pos, "d4e4,e4d4", pureReversal);
    check(!ok, "pure pass-equivalent reversal was incorrectly accepted as a legal turn");

    // Check perft
    uint64_t nodes = compound_perft(pos, 1, false);
    check(nodes > 0, "compound_perft returned 0 nodes");
}

#endif

void extinction_color_settings() {
    Position pos;
    StateListPtr states;

    set_position(pos, states, "asym-extinction-audit",
                 "4k3/8/8/3p4/3Q4/8/8/4K3 w - - 0 1");
    Move capture = parse_move(pos, "d4d5");
    check(pos.see_ge(capture, VALUE_ZERO + 1),
          "SEE ignored the opponent color's losing extinction result");

    set_position(pos, states, "asym-extinction-blast-audit",
                 "4k3/8/8/3p4/3Q4/8/8/4K3 w - - 0 1");
    capture = parse_move(pos, "d4d5");
    check(pos.see_pruning_unreliable()
          && pos.see_pruning_unreliable(capture),
          "SEE pruning was not disabled for the configured opponent goal");
    check(pos.blast_see(capture) >= VALUE_KNOWN_WIN,
          "blast SEE ignored the opponent color's extinction result");
}

void locust_all() {
    Position pos;
    StateListPtr states;
    set_position(pos, states, "locust-all-audit",
                 "7k/8/8/3p4/3p4/3D4/8/K7 w - - 0 1");

    Move move = parse_move(pos, "d3d6");
    check(pos.capture(move), "locust_all move was not classified as a capture");
    check(pos.capture_square(move) == SQ_D4 && pos.captured_piece(move) == make_piece(BLACK, PAWN),
          "locust_all primary capture metadata disagreed");
    check(pos.see_ge(move, Value(150)),
          "locust_all SEE ignored the second captured hurdle");
    check(pos.jump_capture_mask(SQ_D3, SQ_D6)
          == (square_bb(SQ_D4) | square_bb(SQ_D5)),
          "locust_all full capture mask disagreed before simulation");
    SimulatedMoveInfo simulated = pos.simulated_move_info(move);
    Bitboard capturedHurdles = square_bb(SQ_D4) | square_bb(SQ_D5);
    check(simulated.captureSquare == SQ_D4,
          "locust_all simulated capture square disagreed");
    check(simulated.removedByEffects & square_bb(SQ_D5),
          "locust_all simulated removal mask missed the bycatch hurdle");
    check(!(simulated.occupiedAfterEffects & capturedHurdles),
          "locust_all simulated occupancy retained a captured hurdle");
    check(simulated.occupiedAfterEffects & square_bb(SQ_D6),
          "locust_all simulated occupancy lost the landing square");

    Key beforeKey = pos.key();
    std::string beforeFen = pos.fen();
    states->emplace_back();
    pos.do_move(move, states->back());
    check(pos.state()->captured.piece.piece == make_piece(BLACK, PAWN)
          && pos.state()->bycatchSquares == square_bb(SQ_D5),
          "locust_all capture metadata did not separate primary and bycatch captures");
    check(pos.piece_on(SQ_D4) == NO_PIECE && pos.piece_on(SQ_D5) == NO_PIECE,
          "locust_all did not remove every captured hurdle");
    Position reloaded;
    StateListPtr reloadedStates;
    set_position(reloaded, reloadedStates, "locust-all-audit", pos.fen().c_str());
    check(reloaded.key() == pos.key(), "locust_all post-move FEN changed the position key");

    pos.undo_move(move);
    states->pop_back();
    check(pos.key() == beforeKey && pos.fen() == beforeFen,
          "locust_all metadata audit failed do/undo restoration");

    Position passivePos;
    StateListPtr passiveStates;
    set_position(passivePos, passiveStates, "locust-all-passive-order",
                 "7k/8/8/2Bn4/3p4/3D4/8/K7 w - - 0 1");
    check(passivePos.blast_pattern(SQ_D5) & square_bb(SQ_C5),
          "locust_all passive regression blast pattern was not adjacent");
    check(type_of(passivePos.piece_on(SQ_D5)) == KNIGHT
          && (passivePos.variant()->blastPassiveTypes & piece_set(KNIGHT)),
          "locust_all passive regression did not configure the burner");
    check(passivePos.simulated_move_info(parse_move(passivePos, "a1a2")).removedByEffects & square_bb(SQ_C5),
          "locust_all passive regression baseline did not blast");
    Move passiveMove = make<NORMAL>(SQ_D3, SQ_D6);
    SimulatedMoveInfo passiveInfo = passivePos.simulated_move_info(passiveMove);
    check(passiveInfo.captureSquare == SQ_D4
          && (passiveInfo.effectOccupancy & square_bb(SQ_D5)),
          "locust_all passive simulation lost the bycatch before effects");
    check(passiveInfo.removedByEffects & square_bb(SQ_D5),
          "locust_all passive simulation missed the locust hurdle");
    check(passiveInfo.removedByEffects & square_bb(SQ_C5),
          "locust_all passive simulation missed the passive blast");
    check(!(passiveInfo.occupiedAfterEffects & (square_bb(SQ_D5) | square_bb(SQ_C5))),
          "locust_all passive simulation retained an effect removal");
    passiveStates->emplace_back();
    passivePos.do_move(passiveMove, passiveStates->back());
    check(passivePos.piece_on(SQ_D5) == NO_PIECE && passivePos.piece_on(SQ_C5) == NO_PIECE,
          "locust_all passive effect result disagreed with simulation");

    Position wallPos;
    StateListPtr wallStates;
    set_position(wallPos, wallStates, "locust-all-audit",
                 "7k/8/8/3*4/3p4/3D4/8/K7 w - - 0 1");
    Move wallMove = make<NORMAL>(SQ_D3, SQ_D6);
    check(wallPos.state()->wallSquares & square_bb(SQ_D5),
          "locust_all wall regression did not configure the wall");
    check(!(wallPos.jump_capture_mask(SQ_D3, SQ_D6) & square_bb(SQ_D5)),
          "locust_all capture mask included a wall");
    check(wallPos.jump_capture_mask(SQ_D3, SQ_D6) & square_bb(SQ_D4),
          "locust_all capture mask lost the piece hurdle");
    SimulatedMoveInfo wallInfo = wallPos.simulated_move_info(wallMove);
    check((wallInfo.occupiedAfterEffects & square_bb(SQ_D5))
          && !(wallInfo.removedByEffects & square_bb(SQ_D5)),
          "locust_all simulation removed a wall hurdle");

    Position deadPos;
    StateListPtr deadStates;
    set_position(deadPos, deadStates, "locust-all-audit",
                 "7k/8/8/3^4/3p4/3D4/8/K7 w - - 0 1");
    check(!(deadPos.jump_capture_mask(SQ_D3, SQ_D6) & square_bb(SQ_D5))
          && (deadPos.jump_capture_mask(SQ_D3, SQ_D6) & square_bb(SQ_D4)),
          "locust_all capture mask included a dead square");

    Position dropPos;
    StateListPtr dropStates;
    set_position(dropPos, dropStates, "crazyhouse",
                 "7k/8/8/3p4/8/8/8/K7[P] w - - 0 1");
    Move captureDrop = make_drop(SQ_D4, PAWN, PAWN);
    dropPos.simulated_move_info(captureDrop);
    dropPos.pseudo_legal(captureDrop);
    dropPos.see_ge(captureDrop, VALUE_ZERO);

    Position checkPos;
    StateListPtr checkStates;
    set_position(checkPos, checkStates, "locust-all-audit",
                 "8/8/8/2Rp1k2/3p4/3D4/8/K7 w - - 0 1");
    check(checkPos.gives_check(make<NORMAL>(SQ_D3, SQ_D6)),
          "locust_all discovered check missed a removed bycatch hurdle");

    Position evasionPos;
    StateListPtr evasionStates;
    set_position(evasionPos, evasionStates, "locust-all-audit",
                 "3K3k/8/8/3r4/3p4/3D4/8/8 w - - 0 1");
    Move evasionMove = make<NORMAL>(SQ_D3, SQ_D6);
    check(evasionPos.evasion_checkers() & square_bb(SQ_D5),
          "locust_all evasion regression did not configure the checker");
    bool generatedEvasion = false;
    for (const auto& candidate : MoveList<EVASIONS>(evasionPos))
        generatedEvasion |= candidate == evasionMove;
    check(generatedEvasion && evasionPos.pseudo_legal(evasionMove),
          "locust_all evasion was rejected when recovered as a TT move");

    Position extinctionPos;
    StateListPtr extinctionStates;
    set_position(extinctionPos, extinctionStates, "locust-all-extinction",
                 "7k/8/8/3p4/3p4/3D4/8/K7 w - - 0 1");
    check(extinctionPos.see_ge(make<NORMAL>(SQ_D3, SQ_D6), Value(250)),
          "locust_all SEE ignored secondary extinction");

    Position atomicPos;
    StateListPtr atomicStates;
    set_position(atomicPos, atomicStates, "locust-all-atomic",
                 "7k/8/3p4/8/3p4/3D4/8/K7 w - - 0 1");
    check(atomicPos.see_ge(make<NORMAL>(SQ_D3, SQ_D7), Value(150)),
          "locust_all atomic SEE ignored secondary capture value");
}

void occupancy() {
    Position pos;
    StateListPtr states;

    set_position(pos, states, "chess", "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
    Move move = parse_move(pos, "e1g1");
    check_simulation_matches_move(pos, states, move, "castling simulation");
    SimulatedMoveInfo info = pos.simulated_move_info(move);
    Bitboard castled = (pos.pieces() ^ square_bb(SQ_E1) ^ square_bb(SQ_H1))
                     | square_bb(SQ_G1) | square_bb(SQ_F1);
    check(info.castling && info.relocatedOccupancy == castled
          && info.occupiedAfterEffects == castled, "castling occupancy disagreed");

    set_position(pos, states, "chess", "7k/8/8/3pP3/8/8/8/K7 w - d6 0 1");
    move = parse_move(pos, "e5d6");
    info = pos.simulated_move_info(move);
    Bitboard ep = (pos.pieces() ^ square_bb(SQ_E5) ^ square_bb(SQ_D5)) | square_bb(SQ_D6);
    check(info.enPassant && info.captureSquare == SQ_D5 && info.relocatedOccupancy == ep,
          "en passant occupancy disagreed");

    set_position(pos, states, "occupancy-rifle",
                 "7k/8/8/3p4/3Q4/8/8/K7 w - - 0 1");
    info = pos.simulated_move_info(parse_move(pos, "d4d5"));
    check(info.rifle && (info.relocatedOccupancy & square_bb(SQ_D4))
          && !(info.relocatedOccupancy & square_bb(SQ_D5)), "rifle occupancy disagreed");

    set_position(pos, states, "occupancy-blast",
                 "7k/8/8/3pN3/3Q4/8/8/K7 w - - 0 1");
    info = pos.simulated_move_info(parse_move(pos, "d4d5"));
    check(info.removedByEffects & square_bb(SQ_E5), "blast occupancy disagreed");

    set_position(pos, states, "occupancy-clone", "7k/8/8/8/8/8/8/K7 w - - 0 1");
    info = pos.simulated_move_info(make<SPECIAL>(SQ_A1, SQ_A2));
    check(info.clone && (info.relocatedOccupancy & square_bb(SQ_A1))
          && (info.relocatedOccupancy & square_bb(SQ_A2)), "clone occupancy disagreed");

    set_position(pos, states, "occupancy-firstmove", "7k/8/8/8/8/8/8/4K3 w E - 0 1");
    info = pos.simulated_move_info(make<SPECIAL>(SQ_E1, SQ_F3, KNIGHT));
    Bitboard first_move = (pos.pieces() ^ square_bb(SQ_E1)) | square_bb(SQ_F3);
    check(!info.stationary && info.relocatedOccupancy == first_move,
          "first-move special was treated as stationary");

    set_position(pos, states, "pairedpawns", "8/8/8/8/8/8/8/8[PPpp] w - - 0 1");
    info = pos.simulated_move_info(parse_move(pos, "P@a2,h2"));
    check(info.paired && info.secondarySquare == SQ_H2
          && info.effectOccupancy == (square_bb(SQ_A2) | square_bb(SQ_H2)),
          "paired placement occupancy disagreed");

    set_position(pos, states, "occupancy-gating",
                 "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQK1NR[HEhe] w KQBCDFGkqbcdfg - 0 1");
    info = pos.simulated_move_info(make_gating<NORMAL>(SQ_B1, SQ_A3, KNIGHT, SQ_B1));
    check(is_gating(make_gating<NORMAL>(SQ_B1, SQ_A3, KNIGHT, SQ_B1))
          && info.gatingSquare == SQ_B1 && (info.placementOccupancy & square_bb(SQ_B1))
          && (info.placementOccupancy & square_bb(SQ_A3)), "gating occupancy disagreed");

    set_position(pos, states, "occupancy-gating-blast",
                 "7k/8/8/8/8/8/8/R3K3[N] w A - 0 1");
    info = pos.simulated_move_info(make_gating<NORMAL>(SQ_A1, SQ_A2, KNIGHT, SQ_A1));
    check((info.effectOccupancy & square_bb(SQ_A1))
          && (info.removedByEffects & square_bb(SQ_A1))
          && !(info.occupiedAfterEffects & square_bb(SQ_A1)),
          "gating effects occupancy disagreed");

    set_position(pos, states, "occupancy-passive-order",
                 "8/8/8/3pnB2/3R4/8/8/8 w - - 0 1");
    info = pos.simulated_move_info(make<NORMAL>(SQ_D4, SQ_D5));
    check((info.removedByEffects & square_bb(SQ_E5))
          && !(info.removedByEffects & square_bb(SQ_F5)), "passive effect ordering disagreed");

    set_position(pos, states, "occupancy-clone-effects",
                 "8/8/8/8/3N4/2b5/8/8 w - - 0 1");
    info = pos.simulated_move_info(make<SPECIAL>(SQ_D4, SQ_F5));
    check(info.clone && (info.removedByEffects & square_bb(SQ_C3)),
          "clone effect ordering disagreed");

    set_position(pos, states, "occupancy-wall", "8/8/8/8/8/8/8/8 w - - 0 1");
    move = make_gating<SPECIAL>(SQ_A1, SQ_A1, NO_PIECE_TYPE, SQ_A1);
    info = pos.simulated_move_info(move);
    check(is_gating(move) && info.gatingSquare == SQ_A1
          && (info.addedPlacements & square_bb(SQ_A1)) && !info.effectOccupancy
          && (info.placementOccupancy & square_bb(SQ_A1)), "wall occupancy disagreed");

    set_position(pos, states, "chess", "r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");
    Position::SimulatedMoveGuard guard(pos, parse_move(pos, "e1g1"));
    Bitboard occupied = (pos.pieces() ^ square_bb(SQ_E1) ^ square_bb(SQ_H1))
                      | square_bb(SQ_G1) | square_bb(SQ_F1);
    check(pos.piece_at(SQ_G1, occupied) == make_piece(WHITE, KING)
          && pos.piece_at(SQ_F1, occupied) == make_piece(WHITE, ROOK)
          && pos.piece_at(SQ_E1, occupied) == NO_PIECE
          && pos.piece_at(SQ_H1, occupied) == NO_PIECE,
          "piece_at disagreed during simulated castling");

    StateInfo* state = pos.state();
    state->wallSquares = square_bb(SQ_E4);
    state->deadSquares = square_bb(SQ_D4);
    occupied = pos.pieces() | square_bb(SQ_E4) | square_bb(SQ_D4) | square_bb(SQ_C4);
    check(pos.piece_at(SQ_E4, occupied) == NO_PIECE
          && pos.piece_at(SQ_D4, occupied) == NO_PIECE
          && pos.piece_at(SQ_C4, occupied) == NO_PIECE,
          "piece_at exposed synthetic occupancy");
}

void state() {
    Position pos;
    StateListPtr states;

    set_position(pos, states, "atomic",
                 "4k3/8/8/8/8/8/1p6/R3K3 b - - 0 1");
    Move move = parse_move(pos, "b2a1q");
    std::string beforeFen = pos.fen();
    Key beforeKey = pos.key();
    states->emplace_back();
    pos.do_move(move, states->back());
    check(pos.state()->bycatchSquares & square_bb(SQ_A1),
          "Atomic promotion blast did not record the promoted mover");
    check(pos.state()->bycatchPieces[SQ_A1].piece() == make_piece(BLACK, QUEEN)
          && pos.state()->bycatchPieces[SQ_A1].promoted()
          && pos.state()->bycatchPieces[SQ_A1].unpromoted() == make_piece(BLACK, PAWN),
          "Atomic promotion blast lost the exact promoted mover state");
    pos.undo_move(move);
    states->pop_back();
    check(pos.fen() == beforeFen && pos.key() == beforeKey,
          "Atomic promotion blast undo mismatch");

    set_position(pos, states, "atomic",
                 "4k3/8/8/8/8/8/1r6/N~:PR2K3 b - - 0 1");
    check(pos.is_promoted(SQ_A1)
          && pos.unpromoted_piece_on(SQ_A1) == make_piece(WHITE, PAWN),
          "Atomic bycatch regression did not load a promoted bystander");
    move = parse_move(pos, "b2b1");
    beforeFen = pos.fen();
    beforeKey = pos.key();
    states->emplace_back();
    pos.do_move(move, states->back());
    check(pos.state()->bycatchPieces[SQ_A1].piece() == make_piece(WHITE, KNIGHT)
          && pos.state()->bycatchPieces[SQ_A1].promoted()
          && pos.state()->bycatchPieces[SQ_A1].unpromoted() == make_piece(WHITE, PAWN),
          "Atomic blast lost an underpromoted bystander's exact state");
    pos.undo_move(move);
    states->pop_back();
    check(pos.fen() == beforeFen && pos.key() == beforeKey
          && pos.piece_on(SQ_A1) == make_piece(WHITE, KNIGHT)
          && pos.is_promoted(SQ_A1)
          && pos.unpromoted_piece_on(SQ_A1) == make_piece(WHITE, PAWN),
          "Atomic underpromoted bystander undo mismatch");

    set_position(pos, states, "pairedpawns", "8/8/8/8/8/8/8/8[PPpp] w - - 0 1");
    move = parse_move(pos, "P@a2,h2");
    states->emplace_back();
    pos.do_move(move, states->back());
    Key expected = Zobrist::noPawns;
    for (Bitboard b = pos.pieces(); b;) {
        Square square = pop_lsb(b);
        Piece piece = pos.piece_on(square);
        if (type_of(piece) == PAWN)
            expected ^= Zobrist::psq[piece][square];
    }
    check(expected == pos.state()->pawnKey, "paired drop pawn key mismatch");
    pos.undo_move(move);
    states->pop_back();
    expected = Zobrist::noPawns;
    for (Bitboard b = pos.pieces(); b;) {
        Square square = pop_lsb(b);
        Piece piece = pos.piece_on(square);
        if (type_of(piece) == PAWN)
            expected ^= Zobrist::psq[piece][square];
    }
    check(expected == pos.state()->pawnKey, "paired drop undo pawn key mismatch");

    set_position(pos, states, "dos-laser-chess",
                 "r(1)s(0)b(2)q(2)kls(0)b(2)r(1)/d(2)m(0)d(2)m(2)pm(3)d(2)m(1)d(2)/9/9/9/9/9/D(0)M(3)D(0)M(1)PM(0)D(0)M(2)D(0)/R(1)B(0)S(0)LKQ(0)B(0)S(0)R(1) w - - 0 1");
    move = parse_move(pos, "b2b3:1b3");
    beforeKey = pos.key();
    states->emplace_back();
    pos.do_move(move, states->back());
    Position recomputed;
    StateListPtr recomputedStates;
    set_position(recomputed, recomputedStates, "dos-laser-chess", pos.fen().c_str());
    check(pos.key() == recomputed.key()
          && pos.state()->materialKey == recomputed.state()->materialKey,
          "DOS laser key or material key mismatch after gating move");
    pos.undo_move(move);
    states->pop_back();
    check(pos.key() == beforeKey, "DOS laser key mismatch after undo");

    struct Case { const char* variant; const char* fen; const char* move; };
    const Case cases[] = {
      {"khet1", "9k/10/10/10/10/10/OO8/9K w - - 0 1", "a2b2+"},
      {"khet1", "9k/10/10/10/10/10/1T8/9K w - - 0 1", "b2c3-"},
      {"pawn-stack", "8/8/8/8/8/8/PP6/8 w - - 0 1", "a2b2+"},
      {"pawn-stack", "8/8/8/8/8/8/1A6/8 w - - 0 1", "b2c3-"},
      {"khet1", "9k/10/10/10/3p6/2S(0)5/10/9K w - - 0 1", "c3d4s"},
      {"dos-laser-chess", "9/9/9/9/9/9/5k3/9/K4L(0)3 w - - 0 1", "f1f1f"},
      {"dos-laser-chess", "8k/9/9/9/9/9/5~Q(0)3/9/K4L(0)3 w - - 0 1", "f1f1f"},
      {"dos-laser-chess", "r(1)s(0)b(2)q(2)kls(0)b(2)r(1)/d(2)m(0)d(2)m(2)pm(3)d(2)m(1)d(2)/9/9/9/9/9/D(0)M(3)D(0)M(1)PM(0)D(0)M(2)D(0)/R(1)B(0)S(0)LKQ(0)B(0)S(0)R(1) w - - 0 1", "e2e3:1d1"},
      {"dos-laser-chess", "k8/9/9/9/5R(1)3/3M(0)5/9/9/K4L(0)3 w - - 0 1", "d4d5:2f5"},
      {"dos-laser-chess", "k8/9/9/9/9/3M(0)5/9/9/K4L(0)3 w - - 0 1", "d4d5:1d5"},
      {"dos-laser-chess", "k8/9/9/9/3m(0)5/3M(0)5/9/9/K4L(0)3 w - - 0 1", "d4d5:1d5"},
      {"dos-laser-chess", "8k/M(0)8/9/9/9/9/9/9/K4L(0)3 w - - 0 1", "a8a9q:2"}
    };
    for (const Case& test : cases) {
        set_position(pos, states, test.variant, test.fen);
        std::string beforeFen = pos.fen();
        Key beforeKey = pos.key();
        Key beforeMaterial = pos.state()->materialKey;
        Key beforePawn = pos.state()->pawnKey;
        Move compound = parse_move(pos, test.move);
        check(pos.pseudo_legal(compound) && pos.legal(compound),
              std::string("compound laser move is not legal: ") + test.move);
        for (int cycle = 0; cycle < 3; ++cycle) {
            states->emplace_back();
            pos.do_move(compound, states->back());
            Position expectedPos;
            StateListPtr expectedStates;
            set_position(expectedPos, expectedStates, test.variant, pos.fen().c_str());
            check(pos.key() == expectedPos.key()
                  && pos.state()->materialKey == expectedPos.state()->materialKey
                  && pos.state()->pawnKey == expectedPos.state()->pawnKey,
                  std::string("compound move state mismatch: ") + test.move);
            pos.undo_move(compound);
            states->pop_back();
            check(pos.fen() == beforeFen && pos.key() == beforeKey
                  && pos.state()->materialKey == beforeMaterial
                  && pos.state()->pawnKey == beforePawn,
                  std::string("compound move undo mismatch: ") + test.move);
        }
    }
}

void royal() {
    struct Case { const char* variant; const char* fen; };
    const Case cases[] = {
      {"khet1", "9k/10/10/10/10/10/OO8/9K w - - 0 1"},
      {"dos-laser-chess", "r(1)s(0)b(2)q(2)kls(0)b(2)r(1)/d(2)m(0)d(2)m(2)pm(3)d(2)m(1)d(2)/9/9/9/9/9/D(0)M(3)D(0)M(1)PM(0)D(0)M(2)D(0)/R(1)B(0)S(0)LKQ(0)B(0)S(0)R(1) w - - 0 1"},
      {"dos-laser-chess", "9/9/9/9/9/9/5k3/9/K4L(0)3 w - - 0 1"}
    };
    for (const Case& test : cases) {
        Position pos;
        StateListPtr states;
        set_position(pos, states, test.variant, test.fen);
        for (const auto& move : MoveList<QUIET_CHECKS>(pos))
            check(pos.gives_check(move), std::string("non-checking quiet move in ") + test.variant);
        if (std::string(test.fen) == "9/9/9/9/9/9/5k3/9/K4L(0)3 w - - 0 1")
            check(MoveList<QUIET_CHECKS>(pos).contains(parse_move(pos, "f1f1f")),
                  "royal-destroying laser fire missing from quiet checks");
    }
}

void adjudication() {
    auto expect_nonterminal = [](Position& pos, const char* label) {
        Value result = VALUE_NONE;
        check(!pos.is_immediate_game_end(result) && !pos.is_optional_game_end(result),
              std::string("unexpected game end in ") + label);
    };

    Position pos;
    StateListPtr states;
    Value result = VALUE_NONE;

    set_position(pos, states, "snort", "8/8/8/7p/7P/8/8/8 w - - 0 1");
    expect_nonterminal(pos, "snort start position");
    check(!has_insufficient_material(WHITE, pos) && !has_insufficient_material(BLACK, pos),
          "snort enclosing-drop material was misclassified");

    set_position(pos, states, "brandub", "7/7/3r3/2r1r2/3R3/7/7 w - - 0 1");
    check(pos.is_immediate_game_end(result) && result < VALUE_ZERO,
          "brandub missing king was not adjudicated as a loss");

    set_position(pos, states, "antiminishogi", "rbsgk/4p/5/P4/KGSBR[] w - - 0 1");
    expect_nonterminal(pos, "antiminishogi start position");

    set_position(pos, states, "anti-losalamos", "rn1knr/pppppp/6/6/PPPPPP/RNQKNR w - - 0 1");
    expect_nonterminal(pos, "anti-losalamos start position");

    set_position(pos, states, "goal-immediate",
                 "k3r3/8/8/8/8/8/8/4K3 w - - 2 1");
    check(pos.is_immediate_game_end(result) && result == VALUE_DRAW,
          "immediate n-move rule did not return a draw");

    set_position(pos, states, "mixed-goal-simul", "ssAB b - - 0 1");
    check(pos.is_immediate_game_end(result) && result == VALUE_DRAW,
          "simultaneous connection goal did not return a draw");

    set_position(pos, states, "simul-flag-extinction-audit",
                 "7f/8/8/8/8/8/8/F7 w - - 0 1");
    check(pos.is_immediate_game_end(result) && result == mate_in(0),
          "default flag/extinction priority did not preserve extinction-first ordering");

    set_position(pos, states, "simul-flag-extinction-flag",
                 "7f/8/8/8/8/8/8/F7 w - - 0 1");
    check(pos.is_immediate_game_end(result) && result == VALUE_DRAW,
          "flag-priority simultaneous adjudication ignored the mover-value policy");

    set_position(pos, states, "simul-flag-extinction-extinction",
                 "7f/8/8/8/8/8/8/F7 w - - 0 1");
    check(pos.is_immediate_game_end(result) && result == mate_in(0),
          "extinction-priority simultaneous adjudication ignored the mover-value policy");

    set_position(pos, states, "seega", "5/5/5/5/1D3[] b - - 0 1");
    check(pos.count_with_hand(WHITE, CUSTOM_PIECE_1) == 1
              && pos.count_with_hand(BLACK, CUSTOM_PIECE_1) == 0,
          "seega extinction audit position did not load the expected pieces");
    const bool seegaEnded = pos.is_immediate_game_end(result);
    check(seegaEnded, "seega extinction audit position was not adjudicated immediately");
    check(result == mate_in(0), "seega extinction audit position had the wrong result");

    set_position(pos, states, "chess",
                 "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    check(!pos.see_pruning_unreliable()
          && !pos.see_pruning_unreliable(parse_move(pos, "e2e4")),
          "orthodox SEE pruning was disabled");

    set_position(pos, states, "kingofthehill",
                 "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    check(pos.see_pruning_unreliable()
          && !pos.see_pruning_unreliable(parse_move(pos, "a2a3")),
          "flag-goal SEE policy was not move scoped");
    set_position(pos, states, "kingofthehill",
                 "k7/8/8/8/8/4K3/8/8 w - - 0 1");
    check(pos.see_pruning_unreliable(parse_move(pos, "e3e4")),
          "flag-entering king move was not protected from SEE pruning");

    set_position(pos, states, "giveaway",
                 "7k/8/8/3p4/4Q3/8/8/K7 w - - 0 1");
    check(pos.see_pruning_unreliable()
          && pos.see_pruning_unreliable(parse_move(pos, "e4d5")),
          "last extinction-piece capture was not protected from SEE pruning");

    set_position(pos, states, "oshi", variants.get("oshi")->startFen.c_str());
    check(pos.see_pruning_unreliable()
          && pos.see_pruning_unreliable(*MoveList<LEGAL>(pos).begin()),
          "points-goal SEE pruning was enabled");

    set_position(pos, states, "cfour", variants.get("cfour")->startFen.c_str());
    check(pos.see_pruning_unreliable()
          && pos.see_pruning_unreliable(*MoveList<LEGAL>(pos).begin()),
          "connection-goal SEE pruning was enabled");

    set_position(pos, states, "prison-no-king", "8/8/8/8/8/8/4P3/8 w - - 0 1");
    expect_nonterminal(pos, "prison promotion without an opponent king");

    set_position(pos, states, "checkers",
                 "8/8/5k2/8/8/2K5/8/8 w - - 0 1");
    const char* repetitionMoves[] = {
      "c3b4", "f6g5", "b4c3", "g5f6",
      "c3b4", "f6g5", "b4c3", "g5f6"
    };
    for (const char* notation : repetitionMoves)
    {
        Move move = parse_move(pos, notation);
        check(pos.legal(move), std::string("checkers repetition move is illegal: ") + notation);
        states->emplace_back();
        pos.do_move(move, states->back());
    }
    check(pos.is_optional_game_end(result) && result == VALUE_DRAW,
          "checkers threefold repetition was not adjudicated as a draw");
}

void board_games() {
    Position pos;
    StateListPtr states;

    struct Case {
        const char* fen;
        Value result;
    };
    const Case results[] = {
      {"*****/*B*B*/*****/*B*B*/***** w - - 12 7", VALUE_MATE},
      {"*****/*B*B*/*****/*B*b*/***** w - - 12 7", VALUE_MATE},
      {"*****/*B*B*/*****/*b*b*/***** w - - 12 7", VALUE_DRAW},
      {"*****/*B*b*/*****/*b*b*/***** w - - 12 7", -VALUE_MATE}
    };
    for (const Case& test : results) {
        set_position(pos, states, "dots-boxes-2x2", test.fen);
        check(public_game_result(pos) == test.result,
              "dots-and-boxes result disagreed with the public game result");
    }

    set_position(pos, states, "dots-boxes-2x2",
                 "***1*/*b*2/***1*/5/*1*1* b - - 4 3");
    std::vector<std::string> moves;
    for (const auto& move : MoveList<LEGAL>(pos))
        moves.push_back(UCI::move(pos, move));
    std::sort(moves.begin(), moves.end());
    const std::vector<std::string> expected = {
      "0000,a2", "0000,b1", "0000,c2", "0000,d1",
      "0000,d3", "0000,d5", "0000,e2", "0000,e4"
    };
    std::string actual;
    for (const std::string& move : moves)
        actual += (actual.empty() ? "" : ",") + move;
    check(moves == expected, "dots-and-boxes pass move set changed: " + actual);
}

void load_config(const std::string& path) {
    std::ifstream file(path);
    check(file.good(), "cannot read variants file: " + path);
    variants.parse_istream<false>(file);
    std::stringstream inline_config(R"INI(
[pairedpawns:chess]
startFen = 8/8/8/8/8/8/8/8[PPpp] w - - 0 1
pieceDrops = true
symmetricDropTypes = p

[occupancy-rifle:chess]
rifleCapture = true

[rifle-key-audit:chess]
rifleCapture = true

[asym-extinction-audit:chess]
checking = false
castling = false
extinctionValueWhite = win
extinctionPieceTypesWhite = q
extinctionAllPieceTypesWhite = false
extinctionValueBlack = loss
extinctionPieceTypesBlack = p
extinctionAllPieceTypesBlack = false

[asym-extinction-blast-audit:asym-extinction-audit]
extinctionValueWhite = none
extinctionPieceTypesWhite = -
blastOnCapture = true

[occupancy-blast:chess]
blastOnCapture = true

[occupancy-clone:chess]
king = -
commoner = k
castling = false
pseudoRoyalTypes = k
pseudoRoyalCount = 64
cloneMoveTypes = k

[occupancy-gating:chess]
gating = true
seirawanGating = true

[occupancy-firstmove:chess]
gating = true
firstMovePieceTypes = k:n
castling = false

[occupancy-gating-blast:chess]
gating = true
seirawanGating = true
blastOnMove = true

[occupancy-passive-order:chess]
checking = false
blastOnCapture = true
blastPassiveTypes = N

[occupancy-clone-effects:chess]
checking = false
cloneMoveTypes = n
blastPassiveTypes = N

[occupancy-wall:chess]
checking = false
wallingRule = edge
wallingWhite = true
wallingBlack = false
wallOrMove = true
wallingRegionWhite = a1

[locust-all-audit:chess]
pieceToCharTable = PNBRQKDFGHS
customPiece1 = d:c{hurdles: 2,2; pre: 1,*; post: 1,1; hurdle_types: enemy,wall,dead; capture: locust_all}R

[locust-all-passive-order:chess]
pieceToCharTable = PNBRQKDFGHS
blastPassiveTypes = n
blastPattern = W
customPiece1 = d:c{hurdles: 2,2; pre: 1,*; post: 1,1; capture: locust_all}R

[locust-all-extinction:chess]
pieceToCharTable = PNBRQKDFGHS
extinctionValue = loss
extinctionPieceTypes = p
extinctionAllPieceTypes = false
customPiece1 = d:c{hurdles: 2,2; pre: 1,*; post: 1,1; capture: locust_all}R

[locust-all-atomic:chess]
pieceToCharTable = PNBRQKDFGHS
blastOnCapture = true
blastPattern = W
customPiece1 = d:c{hurdles: 2,2; pre: 1,*; post: 1,1; capture: locust_all}R

[pawn-stack:fairy]
laserGame = true
checking = false
king = -
castling = false
customPiece1 = a:K
stackedPieceType = p:a
startFen = 8/8/8/8/8/8/PP6/8 w - - 0 1

[goal-immediate:fairy]
nMoveRuleImmediate = 1
nMoveRule = 0
startFen = k3r3/8/8/8/8/8/8/4K3 w - - 2 1

[mixed-goal-simul:fairy]
maxFile = d
maxRank = 1
pieceToCharTable = -
king = -
customPiece1 = a:m
customPiece2 = b:m
customPiece3 = s:m
connectGoalByType = true
connectPieceGoalWhite = a b
connectPieceGoalBlack = a a
connectPieceTypes = s
connectHorizontal = true
connectVertical = false
connectDiagonal = false
connectRegion1Black = a1
connectRegion2Black = b1
connectGoalSimulValueByMover = draw
startFen = ssAB b - - 0 1

[simul-flag-extinction-audit:fairy]
pieceToCharTable = RF rf
pawn = -
knight = -
bishop = -
rook = -
queen = -
king = -
customPiece1 = r:mW
customPiece2 = f:mW
castling = false
checking = false
flagPieceTypes = f
flagRegionWhite = a1
flagRegionBlack = h8
extinctionPieceTypes = r
extinctionValue = loss
startFen = 7f/8/8/8/8/8/8/F7 w - - 0 1

[simul-flag-extinction-flag:simul-flag-extinction-audit]
simulFlagExtinctionPriority = flag
simulFlagValueByMover = draw
simulExtinctionValueByMover = win

[simul-flag-extinction-extinction:simul-flag-extinction-audit]
simulFlagExtinctionPriority = extinction
simulFlagValueByMover = win
simulExtinctionValueByMover = loss

[prison-no-king:fairy]
king = -
checking = false
prisonPawnPromotion = true
startFen = 8/8/8/8/8/8/4P3/8 w - - 0 1

[dots-boxes-2x2:fairy]
maxRank = 5
maxFile = e
startFen = *1*1*/5/*1*1*/5/*1*1* w - - 0 1
king = -
immobile = b
wallingRule = static
wallOrMove = true
wallingRegion = b1 d1 a2 c2 e2 b3 d3 a4 c4 e4 b5 d5
surroundClaimRegion = b2 d2 b4 d4
surroundClaimPiece = b
surroundClaimExtraTurn = true
materialCounting = unweighted
materialCountingPieceTypes = b

[composable-freeze-traps:fairy]
pieceToCharTable = -
pawn = -
knight = -
bishop = -
rook = -
queen = -
king = -
fers = -
silver = -
aiwok = -
archbishop = -
customPiece1 = e:mW
customPiece2 = m:mW
customPiece3 = r:R
castling = false
checking = false
freezePieceTypes = e
trapRegion = c3 f3 c6 f6
trapProtection = friendly-orthogonal
startFen = 8/8/8/8/8/8/8/8 w - - 0 1

[composable-freeze-orth-only:composable-freeze-traps]
freezeDiagonals = false

[composable-freeze-immune:composable-freeze-traps]
freezeImmunePieceTypes = m

[composable-pass-freeze:chess]
king = -
customPiece1 = f:W
freezePieceTypes = f
pass = true
checking = false
castling = false
startFen = 8/8/8/8/8/8/8/8 w - - 0 1

[composable-freeze-wall:composable-pass-freeze]
pass = false
wallingRule = edge
wallOrMove = true
wallingRegion = h8

[composable-check-morph-in:chess]
moveMorphPieceType = b:r
castling = false

[composable-check-morph-out:chess]
moveMorphPieceType = r:b
castling = false

[composable-pseudoroyal-morph-in:chess]
pseudoRoyalTypes = r
pseudoRoyalCount = 1
moveMorphPieceType = b:r
castling = false

[composable-freeze-traps-blast:fairy]
pieceToCharTable = -
pawn = -
knight = -
bishop = -
rook = -
queen = q
king = -
fers = -
silver = -
aiwok = -
archbishop = -
customPiece1 = e:mW
customPiece2 = r:R
castling = false
checking = false
promotedPieceType = e:q
blastOnMove = true
blastOnCapture = true
blastPromotion = true
trapRegion = e4
trapProtection = friendly-orthogonal
startFen = 8/8/8/8/8/8/8/8 w - - 0 1

[composable-blast-promotion-color:composable-freeze-traps-blast]
changingColorTrigger = capture
changingColorPieceTypes = q
checking = false
trapRegion = -

[composable-rifle-morph-out:chess]
rifleCapture = true
moveMorphPieceType = q:r
blastOnCapture = true
blastImmuneTypes = q
castling = false

[composable-blast-immune:composable-freeze-traps-blast]
blastImmuneTypes = r

[composable-rifle-morph:chess]
rifleCapture = true
moveMorphPieceType = r:q
blastOnCapture = true
blastImmuneTypes = q
castling = false

[composable-ep-ghost:spell-chess]
freezePieceTypes = r

[composable-trap-swap:chess]
adjacentSwapMoveTypes = k
checking = false
trapRegion = f1
trapProtection = none

[composable-wrap-trap:fairy]
maxRank = 1
maxFile = h
toroidal = true
pieceToCharTable = -
pawn = -
knight = -
bishop = -
rook = -
queen = -
king = -
customPiece1 = r:mW
checking = false
trapRegion = b1
trapProtection = friendly-orthogonal
startFen = 8 w - - 0 1

[composable-gated-pawn-blast:chess]
gating = true
blastOnMove = true
checking = false
castling = false

[composable-demotion-morph:chess]
pieceDemotion = true
moveMorphPieceType = p:q
freezePieceTypes = q
checking = false
castling = false

[composable-blast-surround:composable-freeze-traps-blast]
surroundCaptureOpposite = true
trapRegion = -

[composable-blast-promotion-check:composable-freeze-traps-blast]
checking = true
allowChecks = false
king = k
trapRegion = -

[composable-print-overlap:spell-chess]
freezePieceTypes = r

[composable-trap-blocker:fairy]
pieceToCharTable = -
pawn = -
knight = -
bishop = -
rook = -
queen = -
king = -
fers = -
silver = -
aiwok = -
archbishop = -
customPiece1 = r:R
castling = false
checking = false
wallingRule = static
wallingRegion = c3
trapRegion = c3
trapProtection = friendly-orthogonal
startFen = 8/8/8/8/8/2*5/8/8 w - - 0 1

[composable-freeze-check:chess]
customPiece1 = f:Q
freezePieceTypes = f
castling = false

[composable-freeze-evasion:chess]
customPiece1 = f:W
freezePieceTypes = f
castling = false

[composable-trap-evasion:chess]
trapRegion = e8
trapProtection = friendly-orthogonal
castling = false

[composable-trap-quiet-check:chess]
trapRegion = e4
trapProtection = friendly-orthogonal
castling = false

[composable-trap-pawn-quiet-check:chess]
trapRegion = e4
trapProtection = friendly-orthogonal
castling = false

[composable-royal-capture-filter:chess]
captureForbiddenBlack = r:k
castling = false

[composable-rex-royal-capture:chess]
captureMorph = true
rexExclusiveMorph = true
captureForbiddenBlack = q:k
castling = false

[composable-freeze-double-evasion:chess]
customPiece1 = f:N
freezePieceTypes = f
castling = false

[composable-rex-blast:chess]
captureMorph = true
rexExclusiveMorph = true
blastOnCapture = true
blastImmuneTypes = q
castling = false

[composable-stack-morph:chess]
customPiece1 = a:W
customPiece2 = b:W
stackedPieceType = a:b
moveMorphPieceType = b:q
freezePieceTypes = b
castling = false

[composable-stack-passive:chess]
customPiece1 = a:W
customPiece2 = b:W
stackedPieceType = a:b
moveMorphPieceType = b:q
blastPassiveTypes = q
blastPattern = W
checking = false
castling = false

[composable-unstack-morph:chess]
customPiece1 = a:W
customPiece2 = b:W
stackedPieceType = a:b
moveMorphPieceType = a:q
freezePieceTypes = q
castling = false

[composable-unstack-passive:chess]
customPiece1 = a:W
customPiece2 = b:W
stackedPieceType = a:b
moveMorphPieceType = a:q
blastPassiveTypes = a
blastPattern = W
checking = false
castling = false

[composable-death-freeze:chess]
freezePieceTypes = q
deathOnCaptureTypes = q
castling = false

[composable-color-freeze:chess]
freezePieceTypes = q
changingColorTrigger = capture
changingColorPieceTypes = q
castling = false

[composable-trap-final-color:chess]
trapRegion = e4
trapProtection = friendly-orthogonal
changingColorTrigger = always
changingColorPieceTypes = r
castling = false

[composable-flip-trap:composable-freeze-traps]
flipEnclosedPieces = ataxx
trapRegion = e4
startFen = 8/8/8/2E1e3/8/8/8/8 w - - 0 1

[composable-blast-promotion-trap-color:composable-freeze-traps-blast]
blastOrthogonals = false
blastDiagonals = true
blastCenter = false
changingColorTrigger = capture
changingColorPieceTypes = e
trapRegion = e4

[composable-blast-promotion-trap-color-mismatch:composable-freeze-traps-blast]
blastOrthogonals = false
blastDiagonals = true
blastCenter = true
changingColorTrigger = always
changingColorPieceTypes = q
trapRegion = e4

[composable-flip-blast:composable-freeze-traps-blast]
flipEnclosedPieces = ataxx
blastOrthogonals = false
blastDiagonals = true
blastCenter = false
trapRegion = -

[composable-flip-surround:composable-freeze-traps]
flipEnclosedPieces = ataxx
surroundCaptureOpposite = true
trapRegion = -

[composable-liberty-trap:go9]
trapRegion = a1
trapProtection = friendly-orthogonal

[composable-trap-final-death:chess]
trapRegion = e4
trapProtection = friendly-orthogonal
deathOnCaptureTypes = r
castling = false

[composable-color-attack:chess]
changingColorTrigger = always
changingColorPieceTypes = r
castling = false

[composable-compact-check:chess]
pieceToCharTable = -
castling = false

[composable-color-promotion:chess]
changingColorTrigger = always
changingColorPieceTypes = q
castling = false

[composable-potion-freeze:chess]
customPiece1 = f:W
potions = true
freezePotion = f
freezeCooldown = 3
potionDropOnOccupied = true
castling = false

[composable-drop-freeze-check:chess]
customPiece1 = f:W
freezePieceTypes = f
castling = false

[composable-drop-trap-check:chess]
trapRegion = e1
trapProtection = none
castling = false

[composable-drop-trap-discovered-check:chess]
trapRegion = a4
trapProtection = none
pieceDrops = true
freeDrops = true
castling = false

[composable-gate-trap:chess]
gating = true
trapRegion = e2
trapProtection = none
castling = false

[composable-hopper-morph:chess]
pieceToCharTable = PNBRQKDB
customPiece1 = d:c{hurdles: 1,1; pre: 1,1; post: 2,2; hurdle_types: wall; hurdle_piece_types: q; capture: dest}R
customPiece2 = b:W
moveMorphPieceType = b:q
castling = false

[composable-blast-janggi-screen:chess]
castling = false
janggiCannon = c
immobile = i
promotedPieceType = c:i r:q
blastOnMove = true
blastPromotion = true

[composable-trap-ep-check:chess]
castling = false
checking = false
trapRegion = a5
trapProtection = friendly-orthogonal

[composable-trap-ep-legality:chess]
trapRegion = d5
trapProtection = friendly-orthogonal
castling = false

[composable-trap-ep-blocker:chess]
trapRegion = a5
trapProtection = friendly-orthogonal
castling = false

[composable-trap-ep-stale:chess]
checking = false
castling = false
trapRegion = a5
trapProtection = none

[composable-trap-claim-undo:chess]
checking = false
castling = false
trapRegion = e4
trapProtection = none
surroundClaimRegion = e4
surroundClaimPiece = p

[composable-trap-claim-royal:chess]
castling = false
trapRegion = e4
trapProtection = none
surroundClaimRegion = e4
surroundClaimPiece = p

[composable-commitgate-castle:chess]
commitGates = true
startFen = n7/4k3/8/8/8/8/8/8/4K2R/4R2R w K - 0 1

[composable-commitgate-castle-blast:composable-commitgate-castle]
blastOnMove = true
blastPromotion = true
blastCenter = false
promotedPieceType = r:q

[composable-laser-freeze:dos-laser-chess]
freezePieceTypes = r

[composable-check-projected-freeze:chess]
checking = false
freezePieceTypes = q

[composable-sacred-static-freeze:chess]
checkedRoyalsIgnoreFreeze = true
freezePieceTypes = q

[composable-castle-freeze-destination:chess]
freezePieceTypes = r

[composable-castle-effects:chess]
trapRegion = g2
trapProtection = none

[composable-royal-path-freeze:chess]
freezePieceTypes = q
royalPieceNoThroughCheck = true

[composable-trap-royal:chess]
castling = true
trapRegion = g1
trapProtection = none

[arimaa-custom-role-audit:arimaa]
pieceToCharTable = RCDHMErcdhme
startFen = 8/8/8/8/8/8/8/8 w - - 0 1
customPiece1 = x:W
customPiece2 = r:fsW
flagPieceTypes = r
extinctionPieceTypes = r
freezeStrength = r:2 x:1 d:3 h:4 m:5 e:6
pushingStrength = r:2 x:1 d:3 h:4 m:5 e:6
pullingStrength = r:2 x:1 d:3 h:4 m:5 e:6

[arimaa-nonsequential-audit:arimaa]
sequentialSetup = false
turnSteps = 0
pushPullRule = none

[arimaa-push-rule-none-audit:arimaa]
pushPullRule = none

[arimaa-push-rule-generic-audit:arimaa]
pushPullRule = generic

[arimaa-pusher-movement-audit:arimaa-custom-role-audit]
flagPieceTypes = x

[arimaa-board-size-audit:arimaa]
maxFile = j
maxRank = 10
startFen = 9r/10/10/10/10/10/10/10/10/R9 w - - 0 1

[arimaa-wrapped-push-audit:arimaa]
cylindrical = true
samePlayerBoardRepetitionIllegal = false

[generic-compound-turn-audit:fairy]
pieceToCharTable = RCDHMErcdhme
pawn = -
knight = -
bishop = -
rook = -
queen = -
king = -
customPiece1 = r:mW
customPiece2 = c:mW
customPiece3 = d:mW
customPiece4 = h:mW
customPiece5 = m:mW
customPiece6 = e:mW
startFen = 8/8/8/8/8/8/8/8 w - - 0 1
castling = false
checking = false
captureForbidden = *:*
doubleStep = false
promotionPieceTypes = -
freezeStrength = r:1 c:2 d:3 h:4 m:5 e:6
pushingStrength = r:1 c:2 d:3 h:4 m:5 e:6
pullingStrength = r:1 c:2 d:3 h:4 m:5 e:6
pushPullRule = two-step
pushFirstColor = them
stepwisePushing = true
turnSteps = 3
pass = false

[generic-compound-turn-two-audit:generic-compound-turn-audit]
turnSteps = 2

[arimaa-pull-turn-two-audit:arimaa-push-rule-generic-audit]
turnSteps = 2

[generic-compound-stalemate-pass-audit:generic-compound-turn-audit]
customPiece1 = r:-
pass = false
passOnStalemate = true
startFen = 8/8/8/3r4/8/8/8/8 w - - 0 1

[generic-compound-optional-boundary-audit:generic-compound-turn-audit]
nMoveRule = 1

[generic-sequential-setup-audit:fairy]
pawn = -
knight = -
bishop = -
rook = -
queen = -
king = -
customPiece1 = r:mW
pieceToCharTable = RCDHMErcdhme
pieceDrops = true
mustDrop = true
dropRegionWhite = *1
dropRegionBlack = *8
sequentialSetup = true
startFen = 8/8/8/8/8/8/8/8[RRRRrrrr] w - - 0 1

[generic-compound-pass-audit:generic-compound-turn-audit]
pass = true
)INI");
    variants.parse_istream<false>(inline_config);
}

} // namespace

int main(int argc, char** argv) {
    try {
        init_test_engine();
        auto is_group = [](const std::string& name) {
            return name == "all" || name == "promotion" || name == "movement"
                || name == "locust-all" || name == "occupancy" || name == "state" || name == "royal"
                || name == "adjudication" || name == "board-games" || name == "composable-rules"
                || name == "extinction-color"
#ifdef ENABLE_COMPOUND_TURNS
                || name == "compound-turns" || name == "arimaa-setup" || name == "arimaa-architecture"
#endif
                ;
        };
        bool first_is_group = argc > 1 && is_group(argv[1]);
        std::string config_path = first_is_group ? "src/variants.ini"
                              : (argc > 1 && std::string(argv[1]) != "--all" ? argv[1] : "src/variants.ini");
        if (!std::ifstream(config_path).good() && config_path == "src/variants.ini")
            config_path = "variants.ini";
        load_config(config_path);
        std::vector<std::string> requested;
        int first_group = first_is_group ? 1 : (argc > 1 && std::string(argv[1]) != "--all" ? 2 : 1);
        for (int i = first_group; i < argc; ++i)
            requested.emplace_back(argv[i]);
        if (requested.empty())
            requested = {"promotion", "movement", "occupancy", "state", "royal", "adjudication", "board-games"};

        const std::vector<std::pair<std::string, Test>> registry = {
          {"promotion", promotion}, {"movement", movement}, {"occupancy", occupancy},
          {"extinction-color", extinction_color_settings},
          {"locust-all", locust_all},
          {"composable-rules", composable_rules},
#ifdef ENABLE_COMPOUND_TURNS
          {"compound-turns", compound_turn_rules},
          {"arimaa-setup", arimaa_setup},
          {"arimaa-architecture", arimaa_architecture},
#endif
          {"state", state}, {"royal", royal}, {"adjudication", adjudication},
          {"board-games", board_games}
        };
        for (const std::string& name : requested) {
            if (name == "all") {
                for (const auto& entry : registry) {
                    entry.second();
                    std::cout << "ok: native " << entry.first << '\n';
                }
                continue;
            }
            auto it = std::find_if(registry.begin(), registry.end(),
                                   [&](const auto& entry) { return entry.first == name; });
            check(it != registry.end(), "unknown native group: " + name);
            it->second();
            std::cout << "ok: native " << name << '\n';
        }
    } catch (const std::exception& error) {
        std::cerr << "engine-rules failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
