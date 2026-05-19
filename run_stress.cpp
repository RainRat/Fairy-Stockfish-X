#include "src/movegen.h"
#include "src/position.h"
#include "src/thread.h"
#include "src/uci.h"
#include "src/variant.h"

#include <iostream>

using namespace Stockfish;

int main() {
    UCI::init(Options);
    Bitboards::init();
    Position::init();
    Bitbases::init();
    Endgames::init();
    Search::init();

    // We can't easily compile this standalone without Makefile because of link deps.
    // Instead, I'll modify the engine itself (e.g. uci.cpp or movegen.cpp) to assert.
}
