#include <cassert>
#include "../src/piece.h"
#include "../src/piece.cpp"

int main(){
    PieceMap map;
    PieceInfo *pi = new PieceInfo();
    // set slider to trigger diagonalOnly path
    pi->slider[0][MODALITY_QUIET] = {{NORTH_EAST,1}};
    pi->slider[0][MODALITY_CAPTURE] = {{NORTH_EAST,1}};
    pi->steps[0][MODALITY_QUIET].clear();
    pi->steps[0][MODALITY_CAPTURE].clear();
    map.add(0, pi);
    assert(pi->diagonalLimitedSlider==true);
    assert(pi->mobilityScaling>=1);
    delete pi;
    return 0;
}
