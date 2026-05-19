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

    // Add custom variant manually if needed or load from file
    return 0;
}
