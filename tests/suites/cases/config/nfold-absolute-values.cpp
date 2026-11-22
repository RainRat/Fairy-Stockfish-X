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

    // Built-in minishogi must fold nFoldValueAbsolute the same way as
    // INI variants: the side to move at the repetition wins because the
    // opponent made the repetition.
    const Variant* minishogi = variants.get("minishogi");
    if (!minishogi
        || minishogi->nFoldValue.get(WHITE) != VALUE_MATE
        || minishogi->nFoldValue.get(BLACK) != -VALUE_MATE)
    {
        std::cerr << "built-in minishogi n-fold absolute values were not mirrored by color\n";
        return 1;
    }
}
