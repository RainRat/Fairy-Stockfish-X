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

void arimaa_foundation() {
    Position pos;
    StateListPtr states;

    set_position(pos, states, "arimaa-foundation",
                 "8/8/8/4e3/4M3/8/7r/8 w - - 0 1");
    Move frozen = make_move(SQ_E4, SQ_E3);
    check(!pos.legal(frozen), "stronger adjacent piece did not freeze the mover");

    set_position(pos, states, "arimaa-foundation",
                 "8/8/8/4e3/3MM3/8/7r/8 w - - 0 1");
    Move protectedPiece = parse_move(pos, "e4e3");
    check(pos.legal(protectedPiece), "friendly adjacent piece did not protect the mover");

    set_position(pos, states, "arimaa-foundation",
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

    set_position(pos, states, "arimaa-foundation",
                 "8/8/8/8/8/1R6/2R4r/8 w - - 0 1");
    Move protectedTrap = parse_move(pos, "c2c3");
    simulated = pos.simulated_move_info(protectedTrap);
    check(simulated.occupiedAfterEffects & square_bb(SQ_C3),
          "friendly adjacent piece did not protect trap occupant");
}

void arimaa_full() {
    Position pos;
    StateListPtr states;
    set_position(pos, states, "arimaa",
                 "7r/8/8/8/8/3cE3/R7/8 w - - 0 1");

    Move push = parse_move(pos, "e3d3,d4");
    check(pos.legal(push), "Arimaa push was rejected");
    check(pos.pseudo_legal(push), "Arimaa push failed pseudo-legal validation");
    const Key before = pos.key();
    const std::string beforeFen = pos.fen();
    states->emplace_back();
    pos.do_move(push, states->back());
    check(pos.piece_on(SQ_D3) == make_piece(WHITE, CUSTOM_PIECES)
          && pos.piece_on(SQ_D4) == make_piece(BLACK, CUSTOM_PIECES + 4),
          "Arimaa push did not move both pieces atomically");
    pos.undo_move(push);
    states->pop_back();
    check(pos.key() == before && pos.fen() == beforeFen,
          "Arimaa push did not restore state exactly on undo");

    set_position(pos, states, "arimaa",
                 "7r/8/8/8/8/3cE3/R7/8 w - - 0 1");
    Move trapPush = parse_move(pos, "e3d3,c3");
    SimulatedMoveInfo simulated = pos.simulated_move_info(trapPush);
    check(simulated.occupiedAfterEffects & square_bb(SQ_D3),
          "Arimaa push simulation lost the mover");
    check(!(simulated.occupiedAfterEffects & square_bb(SQ_C3)),
          "Arimaa push simulation did not apply the trap after both pieces moved");
    states->emplace_back();
    pos.do_move(trapPush, states->back());
    check(pos.piece_on(SQ_D3) == make_piece(WHITE, CUSTOM_PIECES)
          && pos.piece_on(SQ_C3) == NO_PIECE,
          "Arimaa push did not apply the trap after both pieces moved");
    pos.undo_move(trapPush);
    states->pop_back();

    set_position(pos, states, "arimaa",
                 "7r/8/8/8/8/3cE3/R7/8 w - - 0 1");
    Move pull = parse_move(pos, "e3e4,d3");
    check(pos.pseudo_legal(pull), "Arimaa pull failed pseudo-legal validation");
    simulated = pos.simulated_move_info(pull);
    check(simulated.occupiedAfterEffects & square_bb(SQ_E4),
          "Arimaa pull simulation lost the mover");
    check(simulated.colorOccupancy[BLACK] & square_bb(SQ_E3),
          "Arimaa pull simulation did not relocate the pulled piece");

    set_position(pos, states, "arimaa",
                 "7r/8/8/8/8/4R3/R7/8 w - - 0 1");
    check(pos.legal(parse_move(pos, "e3f3")),
          "Arimaa rabbit sideways move was rejected");
    check(!pos.legal(make_move(SQ_E3, SQ_E2)),
          "Arimaa rabbit backward move was accepted");
    check(!pos.legal(make_move(SQ_E3, SQ_F4)),
          "Arimaa diagonal move was accepted");

    set_position(pos, states, "arimaa",
                 "7r/8/3c4/8/8/4E3/R7/8 w - - 0 1");
    for (const char* notation : {"e3e4", "e4e5", "e5e6"})
    {
        Move m = parse_move(pos, notation);
        check(pos.legal(m), "Arimaa setup step for overrun test was rejected");
        states->emplace_back();
        pos.do_move(m, states->back());
    }
    check(!pos.legal(make_pull(SQ_E6, SQ_D6, SQ_C6)),
          "Arimaa push was allowed to overrun the four-step turn limit");

    set_position(pos, states, "arimaa",
                 "7r/8/8/8/8/4E3/R7/8 w - - 0 1");
    Move firstStep = parse_move(pos, "e3e4");
    states->emplace_back();
    pos.do_move(firstStep, states->back());
    Key oneStepKey = pos.key();
    pos.undo_move(firstStep);
    states->pop_back();

    std::array<Move, 3> repeatedSteps;
    for (int i = 0; i < 3; ++i)
    {
        repeatedSteps[i] = parse_move(pos, i == 1 ? "e4e3" : "e3e4");
        states->emplace_back();
        pos.do_move(repeatedSteps[i], states->back());
    }
    check(pos.piece_on(SQ_E4) == make_piece(WHITE, CUSTOM_PIECES)
          && pos.key() != oneStepKey,
          "Arimaa step count was omitted from the position key");
    for (auto it = repeatedSteps.rbegin(); it != repeatedSteps.rend(); ++it)
    {
        pos.undo_move(*it);
        states->pop_back();
    }

    Move out = parse_move(pos, "e3e4");
    states->emplace_back();
    pos.do_move(out, states->back());
    Move back = parse_move(pos, "e4e3");
    states->emplace_back();
    pos.do_move(back, states->back());
    check(!pos.legal(make<SPECIAL>(SQ_E4, SQ_E4)),
          "Arimaa accepted a voluntary pass equivalent to the turn start");
    pos.undo_move(back);
    states->pop_back();
    pos.undo_move(out);
    states->pop_back();

    for (const char* notation : {"e3e4", "e4e3", "e3e4"})
    {
        Move m = parse_move(pos, notation);
        states->emplace_back();
        pos.do_move(m, states->back());
    }
    check(!pos.legal(make_move(SQ_E4, SQ_E3)),
          "Arimaa accepted a four-step turn equivalent to passing");

    set_position(pos, states, "arimaa",
                 "7r/8/8/8/8/3cE3/R7/8 w - - 0 1");

    Move partial = parse_move(pos, "e3e4");
    check(pos.legal(partial), "Arimaa partial turn step was rejected");
    states->emplace_back();
    pos.do_move(partial, states->back());
    Move endTurn = parse_move(pos, "0000");
    check(pos.legal(endTurn), "Arimaa voluntary turn end was rejected");
    states->emplace_back();
    pos.do_move(endTurn, states->back());
    check(pos.side_to_move() == BLACK && pos.turn_steps() == 0,
          "Arimaa voluntary turn end did not switch sides");
    pos.undo_move(endTurn);
    states->pop_back();
    pos.undo_move(partial);
    states->pop_back();

    std::array<Move, 4> steps;
    for (int i = 0; i < 4; ++i)
    {
        const std::string from = "e" + std::to_string(3 + i);
        const std::string to = "e" + std::to_string(4 + i);
        const std::string notation = from + to;
        Move m = parse_move(pos, notation.c_str());
        steps[i] = m;
        check(pos.legal(m), "Arimaa ordinary step was rejected");
        states->emplace_back();
        pos.do_move(m, states->back());
    }
    check(pos.side_to_move() == BLACK && pos.turn_steps() == 0,
          "Arimaa did not end the turn after four steps");
    for (auto it = steps.rbegin(); it != steps.rend(); ++it)
    {
        pos.undo_move(*it);
        states->pop_back();
    }
    check(pos.side_to_move() == WHITE && pos.fen() == beforeFen,
          "Arimaa turn state did not restore on undo");
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

[arimaa-foundation:fairy]
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
customPiece3 = h:mW
customPiece4 = d:mW
customPiece5 = c:mW
customPiece6 = r:fsmW
castling = false
checking = false
strengthOrder = r c d h m e
freezeRule = stronger-adjacent
freezeProtection = friendly-orthogonal
trapRegion = c3 f3 c6 f6
trapProtection = friendly-orthogonal
startFen = 8/8/8/8/8/8/8/8 w - - 0 1
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
                || name == "adjudication" || name == "board-games" || name == "arimaa"
                || name == "arimaa-full";
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
          {"arimaa", arimaa_foundation},
          {"arimaa-full", arimaa_full},
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
