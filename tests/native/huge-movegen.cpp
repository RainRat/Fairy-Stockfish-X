#include <array>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <tuple>
#include <vector>

#include "movepick.h"
#include "test-support.hpp"

using namespace Stockfish;
static_assert(BOARD_BITS == 512, "This test requires Huge boards");
using Board = std::array<char, 484>;
using KeyTuple = std::tuple<int, int, int>;
const std::array<std::pair<int, int>, 8> directions = {{{-1,-1}, {-1,0}, {-1,1},
    {0,-1}, {0,1}, {1,-1}, {1,0}, {1,1}}};

int step(int square, int dx, int dy) {
    int x = square % 22 + dx, y = square / 22 + dy;
    return x >= 0 && x < 22 && y >= 0 && y < 22 ? y * 22 + x : -1;
}

std::string fen(const Board& board) {
    std::string result;
    for (int y = 21; y >= 0; --y) {
        int empty = 0;
        for (int x = 0; x < 22; ++x) {
            char piece = board[y * 22 + x];
            if (!piece) { ++empty; continue; }
            if (empty) result += std::to_string(empty);
            empty = 0;
            result += piece;
        }
        if (empty) result += std::to_string(empty);
        if (y) result += '/';
    }
    return result + " w - - 0 1";
}

void setup(Position& pos, StateInfo& state, const std::string& name,
           const std::string& rules, const Board& board) {
    std::istringstream config("[" + name + ":chess]\nmaxFile = v\nmaxRank = 22\n"
        "king = -\ncastling = false\nchecking = false\npieceToCharTable = -\n" + rules
        + "startFen = " + fen(board) + "\n");
    variants.parse_istream<false>(config);
    assert(variants.get(name));
    UCI::init_variant(variants.get(name));
    pos.set(variants.get(name), fen(board), false, &state, nullptr);
}

void picker(const Position& pos, const std::set<uint64_t>& expected) {
    auto main = std::make_unique<ButterflyHistory>();
    auto gate = std::make_unique<GateHistory>();
    auto low = std::make_unique<LowPlyHistory>();
    auto captures = std::make_unique<CapturePieceToHistory>();
    auto continuation = std::make_unique<PieceToHistory>();
    main->fill(0); gate->fill(0); low->fill(0); captures->fill(0); continuation->fill(0);
    const PieceToHistory* histories[6] = {continuation.get(), continuation.get(), continuation.get(),
        continuation.get(), continuation.get(), continuation.get()};
    Move killers[2] = {MOVE_NONE, MOVE_NONE};
    MovePicker moves(pos, MOVE_NONE, 1, main.get(), gate.get(), low.get(), captures.get(),
                     histories, MOVE_NONE, killers, 0);
    std::set<uint64_t> picked;
    for (Move move; (move = moves.next_move()) != MOVE_NONE; )
        if (pos.legal(move)) assert(picked.insert(move_bits(move)).second);
    assert(picked == expected);
}

void routes() {
    Board board{};
    const int from = 10 * 22 + 10;
    board[from] = 'A';
    for (auto [dx, dy] : std::array<std::pair<int,int>, 5>{{{-1,0}, {0,1}, {1,0}, {-1,-1}, {0,-1}}})
        board[step(from, dx, dy)] = 'p';
    std::map<KeyTuple, Board> expected;
    auto add = [&](int kind, int via, int to) {
        int captureVia = kind == 0 && board[via] == 'p' ? via : 484;
        Board after = board;
        after[from] = 0;
        if (captureVia != 484) after[captureVia] = 0;
        after[to] = 'A';
        expected[{kind, captureVia, to}] = after;
    };
    for (auto [dx1, dy1] : directions)
        for (auto [dx2, dy2] : directions) {
            int via = step(from, dx1, dy1), to = via < 0 ? -1 : step(via, dx2, dy2);
            if (to >= 0) add(0, via, to);
        }
    for (auto [dx1, dy1] : directions)
        if (dx1 && dy1)
            for (auto [dx2, dy2] : directions)
                if (!dx2 || !dy2)
                    for (int via = step(from, dx1, dy1); via >= 0 && !board[via]; via = step(via, dx1, dy1))
                        for (int to = step(via, dx2, dy2); to >= 0 && board[to] != 'A'; to = step(to, dx2, dy2)) {
                            add(1, via, to);
                            if (board[to]) break;
                        }
    assert(expected.size() == 536 && expected.size() > 512);
    Position pos; StateInfo state;
    setup(pos, state, "huge-routes", "customPiece1 = a:0\ntwoStepMoves = a:*\nhookMoves = a:B-R\n", board);
    std::set<KeyTuple> seen;
    std::set<uint64_t> encoded;
    const std::string before = pos.fen();
    const Key beforeKey = pos.key();
    for (Move move : MoveList<LEGAL>(pos)) {
        assert(is_two_leg(move));
        int via = int(via_sq(move)), to = int(to_sq(move));
        KeyTuple key{is_hook(move) ? 1 : 0, is_two_step(move) && board[via] == 'p' ? via : 484, to};
        assert(seen.insert(key).second && expected.count(key));
        encoded.insert(move_bits(move));
        StateInfo next;
        pos.do_move(move, next);
        for (int square = 0; square < 484; ++square) {
            char wanted = expected.at(key)[square];
            Piece piece = pos.piece_on(Square(square));
            assert(wanted == 'A' ? piece == make_piece(WHITE, CUSTOM_PIECE_1)
                : wanted == 'p' ? piece == B_PAWN : piece == NO_PIECE);
        }
        pos.undo_move(move);
        assert(pos.fen() == before && pos.key() == beforeKey);
    }
    assert(seen.size() == expected.size());
    picker(pos, encoded);
    std::cout << "536 combined routes match independent moves, resulting positions and move picker\n";
}

std::vector<int> queen_moves(const Board& board, int from) {
    std::vector<int> moves;
    for (auto [dx, dy] : directions)
        for (int to = step(from, dx, dy); to >= 0 && !board[to]; to = step(to, dx, dy))
            moves.push_back(to);
    return moves;
}

void stress() {
    Board board{};
    Position pos; StateInfo state;
    std::string rules = "pawn = -\nknight = -\nbishop = -\nrook = -\nqueen = -\n"
        "promotionPieceTypes = -\npieceDrops = true\nfreeDrops = true\n";
    for (int i = 0; i < 78; ++i)
        rules += "customPiece" + std::to_string(i + 1) + " = " + char('a' + i % 26)
               + (i < 26 ? "" : i < 52 ? "'" : "\"") + ":0\n";
    setup(pos, state, "huge-drops", rules, board);
    std::set<uint64_t> expected;
    for (int i = 0; i < 78; ++i)
        for (int square = 0; square < 484; ++square)
            expected.insert(move_bits(make_drop(Square(square), PieceType(CUSTOM_PIECE_1 + i),
                                               PieceType(CUSTOM_PIECE_1 + i))));
    std::set<uint64_t> actual;
    for (Move move : MoveList<LEGAL>(pos)) assert(actual.insert(move_bits(move)).second);
    assert(actual == expected && actual.size() == 37752);
    picker(pos, expected);
    std::cout << "37752 free drops match independent enumeration and move picker (capacity "
              << MOVEGEN_OVERFLOW_CAPACITY << ")\n";

    for (int y : {2, 8, 14, 20})
        for (int x : {2, 6, 10, 14, 18}) board[y * 22 + x] = 'Q';
    setup(pos, state, "huge-arrows", "wallingRule = arrow\n", board);
    expected.clear();
    for (int from = 0; from < 484; ++from)
        if (board[from])
            for (int to : queen_moves(board, from)) {
                Board after = board;
                after[from] = 0; after[to] = 'Q';
                for (int arrow : queen_moves(after, to))
                    expected.insert(move_bits(make_gating<NORMAL>(Square(from), Square(to), NO_PIECE_TYPE, Square(arrow))));
            }
    actual.clear();
    for (Move move : MoveList<LEGAL>(pos)) assert(actual.insert(move_bits(move)).second);
    assert(actual == expected && actual.size() > 10000);
    picker(pos, expected);
    std::cout << actual.size() << " arrow moves match independent queen rays and move picker\n";
}

int main() {
    init_test_engine();
    routes();
    stress();
}
