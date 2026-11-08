#include <iostream>
#include <sstream>

#include "test_engine_init.hpp"

using namespace Stockfish;

int main() {
    init_test_engine();

    std::istringstream config(R"ini(
[nfold-white:chess]
nFoldValueWhite = win
nFoldValueAbsolute = true

[nfold-black:chess]
nFoldValueBlack = win
nFoldValueAbsolute = true
)ini");
    variants.parse_istream<false>(config);

    const Variant* white = variants.get("nfold-white");
    const Variant* black = variants.get("nfold-black");
    if (!white || !black
        || white->nFoldValue.get(WHITE) != VALUE_MATE
        || white->nFoldValue.get(BLACK) != -VALUE_MATE
        || black->nFoldValue.get(WHITE) != -VALUE_MATE
        || black->nFoldValue.get(BLACK) != VALUE_MATE)
    {
        std::cerr << "absolute n-fold values were not mirrored by color\n";
        return 1;
    }
}
