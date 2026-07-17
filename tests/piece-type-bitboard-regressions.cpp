#include <cassert>

#include "types.h"

using namespace Stockfish;

int main() {
    PieceTypeBitboardGroup group(Bitboard(1));
    group.set('A', Bitboard(2));
    group.set('Z', Bitboard(4));
    group |= Bitboard(8);

    assert(group.boardOfPiece('A') == Bitboard(10));
    assert(group.boardOfPiece('Z') == Bitboard(12));
    assert(group.boardOfPiece('B') == Bitboard(9));
    assert(group.explicitBoardOfPiece('B') == Bitboard(0));
    assert(group.anySet());

    PieceTypeBitboardGroup copy = group;
    assert(copy.boardOfPiece('A') == Bitboard(10));
    assert(copy.boardOfPiece('B') == Bitboard(9));
    assert(copy.boardOfPiece('Z') == Bitboard(12));
}
