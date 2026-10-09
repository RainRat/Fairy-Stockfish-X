#include <iostream>
#include <stdexcept>
#include <string>

extern "C" {
using fsf_board = void*;
void fsf_init();
int fsf_try_load_variant_config(const char* content);
fsf_board fsf_new_board(const char* variant, const char* fen, bool is960);
void fsf_free_board(fsf_board board);
const char* fsf_legal_moves(fsf_board board);
bool fsf_has_insufficient_material(fsf_board board, bool turnColor);
bool fsf_is_insufficient_material(fsf_board board);
bool fsf_is_game_over(fsf_board board, bool claimDraw);
const char* fsf_result(fsf_board board, bool claimDraw);
void fsf_free(const char* value);
}

namespace {

void check(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}

std::string take_string(const char* value) {
    check(value != nullptr, "DLL returned a null string");
    std::string result(value);
    fsf_free(value);
    return result;
}

} // namespace

int main() {
    try {
        fsf_init();
        check(fsf_try_load_variant_config(
                  "[dll-no-royal-result:chess]\n"
                  "king = -\n"
                  "castling = false\n"
                  "pass = true\n"
                  "nMoveRule = 0\n"
                  "startFen = 8/8/8/8/8/8/8/8 w - - 0 1\n") == 1,
              "failed to register no-royal pass variant");

        fsf_board board = fsf_new_board("dll-no-royal-result", nullptr, false);
        check(board != nullptr, "DLL failed to create no-royal pass board");
        check(take_string(fsf_legal_moves(board)) == "0000",
              "no-royal pass position did not expose its legal pass");
        check(fsf_has_insufficient_material(board, true)
                  && fsf_has_insufficient_material(board, false),
              "no-royal position did not satisfy both per-side insufficient-material checks");
        check(!fsf_is_insufficient_material(board),
              "DLL classified a no-royal position as an insufficient-material draw");
        check(!fsf_is_game_over(board, false) && !fsf_is_game_over(board, true),
              "DLL marked a playable no-royal position as game over");
        check(take_string(fsf_result(board, false)) == "*"
                  && take_string(fsf_result(board, true)) == "*",
              "DLL inferred a result for a playable no-royal position");
        fsf_free_board(board);
    } catch (const std::exception& error) {
        std::cerr << "ffishdll result regression failed: " << error.what() << '\n';
        return 1;
    }
    std::cout << "ok: DLL no-royal result\n";
}
