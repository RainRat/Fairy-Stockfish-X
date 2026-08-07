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
                 "4k3/8/8/8/8/8/4B3/4K3 w - - 0 1");
    Move checkMorphIn = make_move(SQ_E2, SQ_E3);
    check(pos.gives_check(checkMorphIn),
          "a move morph into a rook was omitted from direct check detection");

    set_position(pos, states, "composable-check-morph-out",
                 "4k3/8/8/8/8/8/4R3/4K3 w - - 0 1");
    Move checkMorphOut = make_move(SQ_E2, SQ_E3);
    check(!pos.gives_check(checkMorphOut),
          "a move morph out of a rook retained a false direct check");

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
          && !(simulated.occupiedAfterEffects & square_bb(SQ_D2)),
          "death-on-capture simulation retained a dead freezer or target");
    {
        Position::SimulatedMoveGuard guard(pos, deathFreezeCapture);
        check(!(pos.freeze_squares(BLACK) & square_bb(SQ_E3)),
              "dead freezer continued neutralizing a checker");
    }
    check(!pos.legal(deathFreezeCapture),
          "move with a dead freezer incorrectly remained legal");

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

[composable-check-morph-in:chess]
moveMorphPieceType = b:r
castling = false

[composable-check-morph-out:chess]
moveMorphPieceType = r:b
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
                || name == "adjudication" || name == "board-games" || name == "composable-rules";
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
