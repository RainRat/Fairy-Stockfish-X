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
    set_position(pos, states, "pairedpawns", "8/8/8/8/8/8/8/8[PPpp] w - - 0 1");
    Move move = parse_move(pos, "P@a2,h2");
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
    Key beforeKey = pos.key();
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

    set_position(pos, states, "prison-no-king", "8/8/8/8/8/8/4P3/8 w - - 0 1");
    expect_nonterminal(pos, "prison promotion without an opponent king");
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
)INI");
    variants.parse_istream<false>(inline_config);
}

} // namespace

int main(int argc, char** argv) {
    try {
        init_test_engine();
        auto is_group = [](const std::string& name) {
            return name == "all" || name == "promotion" || name == "movement"
                || name == "occupancy" || name == "state" || name == "royal"
                || name == "adjudication" || name == "board-games";
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
