#include <atomic>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

extern "C" {
using fsf_board = void*;
void fsf_init();
int fsf_try_load_variant_config(const char* content);
fsf_board fsf_new_board(const char* variant, const char* fen, bool is960);
void fsf_free_board(fsf_board board);
void fsf_set_option_bool(const char* name, bool value);
const char* fsf_legal_moves(fsf_board board);
const char* fsf_legal_moves_san(fsf_board board);
int fsf_number_legal_moves(fsf_board board);
bool fsf_push(fsf_board board, const char* uci);
bool fsf_push_san(fsf_board board, const char* san, int notation);
void fsf_pop(fsf_board board);
void fsf_reset(fsf_board board);
const char* fsf_fen(fsf_board board, bool showPromoted, int countStarted);
bool fsf_set_fen(fsf_board board, const char* fen);
const char* fsf_san_move(fsf_board board, const char* uci, int notation);
const char* fsf_variation_san(fsf_board board, const char* moves, int notation, bool moveNumbers);
const char* fsf_variant_info(const char* variant);
bool fsf_turn(fsf_board board);
int fsf_fullmove_number(fsf_board board);
int fsf_halfmove_clock(fsf_board board);
int fsf_game_ply(fsf_board board);
bool fsf_has_insufficient_material(fsf_board board, bool turnColor);
bool fsf_is_insufficient_material(fsf_board board);
bool fsf_is_game_over(fsf_board board, bool claimDraw);
const char* fsf_result(fsf_board board, bool claimDraw);
const char* fsf_checked_pieces(fsf_board board);
const char* fsf_evasion_checked_pieces(fsf_board board);
bool fsf_is_check(fsf_board board);
bool fsf_is_real_check(fsf_board board);
bool fsf_is_bikjang(fsf_board board);
bool fsf_is_capture(fsf_board board, const char* uci);
bool fsf_try_is_capture(fsf_board board, const char* uci, bool* result);
const char* fsf_move_stack(fsf_board board);
void fsf_push_moves(fsf_board board, const char* moves);
void fsf_push_san_moves(fsf_board board, const char* moves, int notation);
bool fsf_try_push_moves(fsf_board board, const char* moves, int* applied);
bool fsf_try_push_san_moves(fsf_board board, const char* moves, int notation, int* applied);
const char* fsf_pocket(fsf_board board, bool color);
const char* fsf_board_string(fsf_board board);
const char* fsf_board_verbose_string(fsf_board board);
const char* fsf_board_variant(fsf_board board);
const char* fsf_available_variants();
int fsf_validate_fen(const char* fen, const char* variant, bool is960);
int fsf_validate_position(const char* fen, const char* variant, bool is960, const char* moves);
const char* fsf_starting_fen(const char* variant);
bool fsf_captures_to_hand(const char* variant);
bool fsf_try_set_option_bool(const char* name, bool value);
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

void check_empty(const char* value, const char* name) {
    check(take_string(value).empty(), std::string(name) + " should return an empty string for a null board");
}

void check_null_board_api() {
    bool capture = true;
    int applied = -1;
    check_empty(fsf_legal_moves(nullptr), "fsf_legal_moves");
    check_empty(fsf_legal_moves_san(nullptr), "fsf_legal_moves_san");
    check(fsf_number_legal_moves(nullptr) == 0, "fsf_number_legal_moves should return zero");
    check(!fsf_push(nullptr, "e2e4") && !fsf_push_san(nullptr, "e4", 1), "push should fail for a null board");
    fsf_pop(nullptr);
    fsf_reset(nullptr);
    check_empty(fsf_fen(nullptr, false, 0), "fsf_fen");
    check(!fsf_set_fen(nullptr, ""), "fsf_set_fen should fail for a null board");
    check_empty(fsf_san_move(nullptr, "e2e4", 1), "fsf_san_move");
    check_empty(fsf_variation_san(nullptr, "e2e4", 1, true), "fsf_variation_san");
    check(!fsf_turn(nullptr) && fsf_fullmove_number(nullptr) == 0
              && fsf_halfmove_clock(nullptr) == 0 && fsf_game_ply(nullptr) == 0,
          "numeric board accessors should return neutral values");
    check(!fsf_has_insufficient_material(nullptr, true) && !fsf_is_insufficient_material(nullptr)
              && !fsf_is_game_over(nullptr, false),
          "material and game-over accessors should return false");
    check_empty(fsf_result(nullptr, false), "fsf_result");
    check_empty(fsf_checked_pieces(nullptr), "fsf_checked_pieces");
    check_empty(fsf_evasion_checked_pieces(nullptr), "fsf_evasion_checked_pieces");
    check(!fsf_is_check(nullptr) && !fsf_is_real_check(nullptr) && !fsf_is_bikjang(nullptr)
              && !fsf_is_capture(nullptr, "e2e4"),
          "check and capture accessors should return false");
    check(!fsf_try_is_capture(nullptr, "e2e4", &capture) && !capture,
          "checked capture accessor did not report a null board");
    check_empty(fsf_move_stack(nullptr), "fsf_move_stack");
    fsf_push_moves(nullptr, "e2e4");
    fsf_push_san_moves(nullptr, "e4", 1);
    check(!fsf_try_push_moves(nullptr, "e2e4", &applied) && applied == 0,
          "checked batch push did not report a null board");
    check_empty(fsf_pocket(nullptr, true), "fsf_pocket");
    check_empty(fsf_board_string(nullptr), "fsf_board_string");
    check_empty(fsf_board_verbose_string(nullptr), "fsf_board_verbose_string");
    check_empty(fsf_board_variant(nullptr), "fsf_board_variant");
    fsf_free_board(nullptr);
}

} // namespace

int main() {
    try {
        const std::string chessFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
        check(take_string(fsf_available_variants()).find("chess") != std::string::npos,
              "variant getter did not initialize the DLL lazily");
        check(take_string(fsf_starting_fen("chess")) == chessFen,
              "start-FEN getter failed before explicit initialization");
        check(!fsf_captures_to_hand("chess"), "capture-to-hand getter failed before explicit initialization");
        check(fsf_validate_fen(chessFen.c_str(), "chess", false) == 1,
              "FEN validation failed before explicit initialization");
        check(fsf_validate_position(chessFen.c_str(), "chess", false, "") == 1,
              "position validation failed before explicit initialization");
        check(fsf_validate_fen(chessFen.c_str(), "missing-variant", false) == -17,
              "unknown variant FEN validation did not return FEN_INVALID_VARIANT");
        check(fsf_validate_position(chessFen.c_str(), "missing-variant", false, "") == -17,
              "unknown variant position validation did not return FEN_INVALID_VARIANT");
        check(fsf_new_board("missing-variant", nullptr, false) == nullptr,
              "unknown variant board creation did not return nullptr");
        check(fsf_new_board("chess", "bad-fen", false) == nullptr,
              "invalid FEN board creation did not return nullptr");
        check_null_board_api();
        check(take_string(fsf_variant_info("chess")).find("\"name\":\"chess\"") != std::string::npos,
              "DLL variant metadata did not match the shared schema");
        fsf_init();
        check(fsf_try_load_variant_config(
                  "[dll-symbol-piece-region:chess]\n"
                  "customPiece1 = z!:K\n"
                  "promotionRegion = Z(a7);\n") == 1,
              "failed to register symbolic metadata variant");
        check(take_string(fsf_variant_info("dll-symbol-piece-region")).find("\"custom1\":[\"a7\"]")
                  != std::string::npos,
              "DLL omitted region data for a symbolic custom piece");
        check(fsf_try_load_variant_config(
                  "[dll-stalemate-claim-precedence:chess]\n"
                  "stalemateValue = loss\n") == 1,
              "failed to register stalemate precedence variant");
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

        board = fsf_new_board("chess", chessFen.c_str(), false);
        check(board != nullptr, "DLL failed to create chess board for API contract checks");
        bool capture = true;
        check(fsf_try_is_capture(board, "e2e4", &capture) && !capture,
              "try_is_capture did not distinguish a valid quiet move");
        check(!fsf_try_is_capture(board, "bad", &capture),
              "try_is_capture accepted a malformed move");
        int applied = -1;
        check(!fsf_try_push_moves(board, "e2e4 bad g1f3", &applied) && applied == 1
                  && take_string(fsf_fen(board, false, 0))
                     == "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1",
              "checked bulk push did not report its applied prefix");
        check(!fsf_try_set_option_bool("UnknownOption", true),
              "checked option setter accepted an unknown option");
        check(fsf_try_set_option_bool("DynamicMagicsByBoardSize", true),
              "checked option setter rejected a known option");
        check(take_string(fsf_variation_san(board, "e7e5 g1f3", 1, true)) == "1...e5 2. Nf3",
              "DLL variation SAN spacing differs from JS");
        fsf_free_board(board);

        board = fsf_new_board("chess", chessFen.c_str(), false);
        int sanApplied = -1;
        check(board && !fsf_try_push_san_moves(board, "e4 ?? Nf3", 1, &sanApplied)
                  && sanApplied == 1
                  && take_string(fsf_fen(board, false, 0))
                     == "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1",
              "checked SAN batch did not report its applied prefix");
        fsf_free_board(board);

        const char* stalemateFen = "7k/5Q2/6K1/8/8/8/8/8 b - - 100 51";
        board = fsf_new_board("dll-stalemate-claim-precedence", stalemateFen, false);
        check(board != nullptr && fsf_number_legal_moves(board) == 0
                  && take_string(fsf_result(board, true)) == "1-0",
              "DLL optional draw took precedence over stalemate loss");
        fsf_free_board(board);

        // Boards share global piece/attack tables; using a board refreshes
        // them for its own variant, so interleaved boards must not corrupt
        // each other (custom knight moves vanished after a plain chess
        // board existed). Mirrors the ffishjs cross-board regression test.
        check(fsf_try_load_variant_config(
                  "[dll-crossknight:chess]\n"
                  "customPiece1 = n:K\n") == 1,
              "failed to register cross-board knight variant");
        {
            const char* knightFen = "4k3/8/8/8/8/1N6/8/4K3 w - - 0 1";
            fsf_board custom = fsf_new_board("dll-crossknight", knightFen, false);
            check(custom != nullptr, "DLL failed to create custom-knight board");
            const std::string before = take_string(fsf_legal_moves(custom));
            check(before.find("b3c4") != std::string::npos
                      && before.find("b3a5") == std::string::npos,
                  "custom knight did not move like a king");
            fsf_board chessBoard = fsf_new_board("chess", knightFen, false);
            check(chessBoard != nullptr, "DLL failed to create chess board");
            const std::string chessMoves = take_string(fsf_legal_moves(chessBoard));
            check(chessMoves.find("b3a5") != std::string::npos
                      && chessMoves.find("b3c4") == std::string::npos,
                  "chess knight moves corrupted by custom-knight board");
            check(take_string(fsf_legal_moves(custom)) == before,
                  "custom-knight moves corrupted by chess board creation");

            // set_fen must rebuild cached checkers/state with the board's
            // own tables: with chess tables current, the custom knight's
            // check would be cached as a non-check. Re-stale the tables
            // with a chess query first (the equality check above re-synced
            // the custom tables).
            check(take_string(fsf_legal_moves(chessBoard)) == chessMoves,
                  "chess moves changed across board interleaving");
            check(fsf_set_fen(custom, "4k3/8/8/8/2K5/1n6/8/8 w - - 0 1"),
                  "DLL rejected a valid custom-knight FEN");
            check(fsf_is_real_check(custom),
                  "custom knight on b3 checking the king on c4 was cached as non-check");
            fsf_free_board(chessBoard);
            fsf_free_board(custom);
        }

        std::atomic<bool> start{false};
        std::atomic<bool> failed{false};
        auto wait_for_start = [&] {
            while (!start.load(std::memory_order_acquire))
                std::this_thread::yield();
        };
        // Concurrent move generation on two pre-created boards of variants
        // whose knight movements differ must observe each board's own
        // tables: refresh and consumption are serialized by the mutex.
        {
            const char* knightFen = "4k3/8/8/8/8/1N6/8/4K3 w - - 0 1";
            fsf_board customBoard = fsf_new_board("dll-crossknight", knightFen, false);
            fsf_board chessBoard = fsf_new_board("chess", knightFen, false);
            check(customBoard != nullptr && chessBoard != nullptr,
                  "DLL failed to pre-create boards for the concurrency test");
            std::thread customReader([&] {
                wait_for_start();
                for (int i = 0; i < 200; ++i) {
                    const std::string moves = take_string(fsf_legal_moves(customBoard));
                    if (moves.find("b3c4") == std::string::npos
                        || moves.find("b3a5") != std::string::npos)
                        failed.store(true, std::memory_order_relaxed);
                }
            });
            std::thread chessReader([&] {
                wait_for_start();
                for (int i = 0; i < 200; ++i) {
                    const std::string moves = take_string(fsf_legal_moves(chessBoard));
                    if (moves.find("b3a5") == std::string::npos
                        || moves.find("b3c4") != std::string::npos)
                        failed.store(true, std::memory_order_relaxed);
                }
            });
            start.store(true, std::memory_order_release);
            customReader.join();
            chessReader.join();
            check(!failed.load(std::memory_order_relaxed),
                  "concurrent move generation observed another board's tables");
            fsf_free_board(chessBoard);
            fsf_free_board(customBoard);
            start.store(false, std::memory_order_release);
        }
        std::thread optionSetter([&] {
            wait_for_start();
            for (int i = 0; i < 200; ++i)
                fsf_set_option_bool("DynamicMagicsByBoardSize", i % 2);
        });
        std::thread boardCreator([&] {
            wait_for_start();
            for (int i = 0; i < 200; ++i) {
                fsf_board concurrentBoard = fsf_new_board("chess", nullptr, false);
                if (!concurrentBoard) {
                    failed.store(true, std::memory_order_relaxed);
                    continue;
                }
                fsf_free_board(concurrentBoard);
            }
        });
        start.store(true, std::memory_order_release);
        optionSetter.join();
        boardCreator.join();
        check(!failed.load(std::memory_order_relaxed),
              "option updates raced with DLL board construction");
    } catch (const std::exception& error) {
        std::cerr << "ffishdll result regression failed: " << error.what() << '\n';
        return 1;
    }
    std::cout << "ok: DLL no-royal result\n";
}
