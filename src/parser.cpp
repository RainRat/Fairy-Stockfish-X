/*
  Fairy-Stockfish, a UCI chess variant playing engine derived from Stockfish
  Copyright (C) 2018-2022 Fabian Fichter

  Fairy-Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Fairy-Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <string>
#include <sstream>
#include <limits>
#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <memory>

#include "apiutil.h"
#include "parser.h"
#include "piece.h"
#include "types.h"

namespace Stockfish {

namespace {
    template <typename T, size_t N>
    bool parse_named_value(const std::string& value, T& target, const std::array<std::pair<const char*, T>, N>& values) {
        const auto first = value.find_first_not_of(" \t\r\n\f\v");
        if (first == std::string::npos)
            return false;
        const auto last = value.find_last_not_of(" \t\r\n\f\v");
        const std::string trimmed = value.substr(first, last - first + 1);
        for (const auto& [name, parsed] : values)
            if (trimmed == name)
            {
                target = parsed;
                return true;
            }
        return false;
    }

    bool only_trailing_space(std::stringstream& ss) {
        ss >> std::ws;
        return ss.eof();
    }

    std::string trim(const std::string& s) {
        const auto first = s.find_first_not_of(" \t");
        if (first == std::string::npos)
            return "";
        const auto last = s.find_last_not_of(" \t");
        return s.substr(first, last - first + 1);
    }

    std::string read_piece_token(const std::string& value) {
        std::string s = trim(value);
        if (s.empty() || !Variant::is_piece_id_start(s[0]))
            return "";
        std::string token(1, s[0]);
        if (s.size() >= 2 && Variant::is_piece_id_suffix(s[1]))
            token.push_back(s[1]);
        return token;
    }

    std::pair<std::string, std::string> split_piece_entry(const std::string& value) {
        std::string s = trim(value);
        std::string token = read_piece_token(s);
        if (token.empty())
            return {"", ""};
        if (s.size() == token.size())
            return {token, ""};
        if (s[token.size()] != ':')
            return {"", ""};
        return {token, s.substr(token.size() + 1)};
    }

    bool parse_positive_int(const std::string& raw, int& out) {
        if (raw.empty())
            return false;

        const auto first = raw.find_first_not_of(" \t\r\n\f\v");
        if (first == std::string::npos)
            return false;
        const auto last = raw.find_last_not_of(" \t\r\n\f\v");
        const char* begin = raw.data() + first;
        const char* end = raw.data() + last + 1;
        auto [ptr, ec] = std::from_chars(begin, end, out);
        return ec == std::errc() && ptr == end && out >= 1;
    }

    bool parse_file_index(const std::string& raw, int& out) {
        if (raw.empty())
            return false;

        const auto first = raw.find_first_not_of(" \t\r\n\f\v");
        if (first == std::string::npos)
            return false;
        const auto last = raw.find_last_not_of(" \t\r\n\f\v");
        const std::string value = raw.substr(first, last - first + 1);

        if (std::isdigit(static_cast<unsigned char>(value[0])))
        {
            int file = 0;
            auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), file);
            if (ec != std::errc() || ptr != value.data() + value.size() || file < 1)
                return false;
            out = file - 1;
            return true;
        }

        if (value.size() != 1)
            return false;
        if (!std::isalpha(static_cast<unsigned char>(value[0])))
            return false;
        out = std::tolower(static_cast<unsigned char>(value[0])) - 'a';
        return true;
    }

    template <typename Apply>
    void parse_color_triplet(const Config& config, const std::string& key, Apply&& apply) {
        if (config.find(key) != config.end())
            apply(key, COLOR_NB);
        const std::string whiteKey = key + "White";
        if (config.find(whiteKey) != config.end())
            apply(whiteKey, WHITE);
        const std::string blackKey = key + "Black";
        if (config.find(blackKey) != config.end())
            apply(blackKey, BLACK);
    }

    PieceType parse_piece_type_token(const Variant* v, const std::string& token) {
        if (token.empty())
            return NO_PIECE_TYPE;
        std::string normalized = token;
        normalized[0] = char(std::toupper(static_cast<unsigned char>(normalized[0])));
        for (PieceType pt = PAWN; pt < PIECE_TYPE_NB; ++pt)
        {
            Piece white = make_piece(WHITE, pt);
            if (v->pieceToSymbol[white] == normalized || v->pieceToSymbolSynonyms[white] == normalized)
                return pt;
        }
        return NO_PIECE_TYPE;
    }

    bool parse_first_piece_type_token(const Variant* v, const std::string& value, PieceType& target) {
        std::string token = read_piece_token(trim(value));
        if (token.empty())
            return false;
        PieceType pt = parse_piece_type_token(v, token);
        if (pt == NO_PIECE_TYPE)
            return false;
        target = pt;
        return true;
    }

    bool parse_piece_int_map(const std::string& value, const Variant* v, int target[PIECE_TYPE_NB], bool allowZero = true) {
        std::string entry;
        int parsedValue = 0;
        std::stringstream ss(value);
        int parsed[PIECE_TYPE_NB];
        std::copy(target, target + PIECE_TYPE_NB, std::begin(parsed));
        bool sawEntry = false;
        while (ss >> entry)
        {
            sawEntry = true;
            auto [token, rawValue] = split_piece_entry(entry);
            PieceType pt = parse_piece_type_token(v, token);
            if (pt == NO_PIECE_TYPE || rawValue.empty())
                return false;
            const char* first = rawValue.data();
            const char* last  = first + rawValue.size();
            auto [ptr, ec] = std::from_chars(first, last, parsedValue);
            while (ptr != last && std::isspace(static_cast<unsigned char>(*ptr)))
                ptr++;
            if (ec != std::errc() || ptr != last)
                return false;
            if (!allowZero && parsedValue < 1)
                return false;
            parsed[pt] = parsedValue;
        }
        if (!sawEntry)
            return false;
        std::copy(std::begin(parsed), std::end(parsed), target);
        return only_trailing_space(ss);
    }

    template <typename OnParsed>
    bool parse_piece_int_map_option(const std::string& optionName,
                                    const std::string& value,
                                    const Variant* v,
                                    int target[PIECE_TYPE_NB],
                                    bool allowZero,
                                    bool DoCheck,
                                    OnParsed&& onParsed) {
        int parsed[PIECE_TYPE_NB];
        std::copy(target, target + PIECE_TYPE_NB, std::begin(parsed));
        if (!parse_piece_int_map(value, v, parsed, allowZero))
        {
            if (DoCheck)
                std::cerr << optionName << " - Invalid syntax." << std::endl;
            return false;
        }
        if (!onParsed(parsed))
            return false;
        std::copy(std::begin(parsed), std::end(parsed), target);
        return true;
    }

    bool parse_piece_type_map(const std::string& value, const Variant* v, PieceType target[PIECE_TYPE_NB], bool allowNone = false) {
        std::string entry;
        std::stringstream ss(value);
        PieceType parsed[PIECE_TYPE_NB];
        std::copy(target, target + PIECE_TYPE_NB, std::begin(parsed));
        bool sawEntry = false;
        while (ss >> entry)
        {
            sawEntry = true;
            auto [fromToken, rawTo] = split_piece_entry(entry);
            PieceType from = parse_piece_type_token(v, fromToken);
            if (from == NO_PIECE_TYPE || rawTo.empty())
                return false;
            PieceType to = rawTo == "-" && allowNone ? NO_PIECE_TYPE : parse_piece_type_token(v, rawTo);
            if (to == NO_PIECE_TYPE && !(allowNone && rawTo == "-"))
                return false;
            parsed[from] = to;
        }
        if (!sawEntry)
            return false;
        std::copy(std::begin(parsed), std::end(parsed), target);
        return only_trailing_space(ss);
    }

    bool parse_piece_set_token_string(const std::string& text, const Variant* v, PieceSet& target, bool allowAll = true, bool allowNone = true);

    bool parse_drop_piece_type_map(const std::string& value, const Variant* v, PieceSet target[PIECE_TYPE_NB]) {
        std::stringstream groups(value);
        std::string group;
        PieceSet parsed[PIECE_TYPE_NB];
        std::copy(target, target + PIECE_TYPE_NB, std::begin(parsed));
        bool sawGroup = false;
        while (std::getline(groups, group, ';'))
        {
            group = trim(group);
            if (group.empty())
                continue;
            sawGroup = true;

            std::stringstream ss(group);
            std::string head;
            if (!(ss >> head))
                return false;

            auto [fromToken, rest] = split_piece_entry(head);
            PieceType from = parse_piece_type_token(v, fromToken);
            if (from == NO_PIECE_TYPE)
                return false;

            PieceSet mask = NO_PIECE_SET;
            std::string rhs = trim(rest);
            if (!rhs.empty())
            {
                if (!parse_piece_set_token_string(rhs, v, mask, true, true))
                    return false;
            }

            std::string token;
            while (ss >> token)
            {
                if (token == "-")
                {
                    mask = NO_PIECE_SET;
                    continue;
                }
                PieceType pt = parse_piece_type_token(v, token);
                if (pt == NO_PIECE_TYPE)
                    return false;
                mask |= pt;
            }

            parsed[from] = mask;
        }
        if (!sawGroup)
            return false;
        std::copy(std::begin(parsed), std::end(parsed), target);
        return true;
    }

    bool parse_piece_set_token_string(const std::string& text, const Variant* v, PieceSet& target, bool allowAll, bool allowNone) {
        std::string remaining = trim(text);
        PieceSet parsed = NO_PIECE_SET;
        if (remaining.empty())
            return false;
        if (allowAll && remaining == "*") {
            target = v->pieceTypes;
            return true;
        }
        if (allowNone && remaining == "-") {
            target = NO_PIECE_SET;
            return true;
        }
        while (!remaining.empty())
        {
            std::string token = read_piece_token(remaining);
            if (token.empty())
                return false;
            PieceType pt = parse_piece_type_token(v, token);
            if (pt == NO_PIECE_TYPE)
                return false;
            parsed |= pt;
            remaining.erase(0, token.size());
            remaining = trim(remaining);
        }
        target = parsed;
        return true;
    }

    template <typename T, typename ParseOne>
    bool parse_named_color_pair(const Config& config,
                                const std::string& whiteKey,
                                const std::string& blackKey,
                                T& whiteTarget,
                                T& blackTarget,
                                bool DoCheck,
                                ParseOne&& parseOne) {
        const auto& itWhite = config.find(whiteKey);
        if (itWhite != config.end() && !parseOne(whiteKey, itWhite->second, whiteTarget))
        {
            if (DoCheck)
                std::cerr << whiteKey << " - Invalid syntax." << std::endl;
            return false;
        }

        const auto& itBlack = config.find(blackKey);
        if (itBlack != config.end() && !parseOne(blackKey, itBlack->second, blackTarget))
        {
            if (DoCheck)
                std::cerr << blackKey << " - Invalid syntax." << std::endl;
            return false;
        }
        return true;
    }

    bool looks_like_piece_definition_value(const std::string& value) {
        return value.size() >= 2
            && std::isalpha(static_cast<unsigned char>(value[0]))
            && value[1] == ':';
    }

    bool parse_rank_index(const std::string& value, int& out) {
        int rank = 0;
        if (!parse_positive_int(value, rank))
            return false;
        out = rank - 1;
        return true;
    }

    bool apply_edge_insert_from_alias(const std::string& value, bool& top, bool& bottom, bool& left, bool& right) {
        std::stringstream ss(value);
        std::string token;
        bool any = false;
        while (ss >> token)
        {
            if (token == "all")
                top = bottom = left = right = true;
            else if (token == "vertical")
                top = bottom = true;
            else if (token == "horizontal")
                left = right = true;
            else if (token == "top")
                top = true;
            else if (token == "bottom")
                bottom = true;
            else if (token == "left")
                left = true;
            else if (token == "right")
                right = true;
            else
                return false;
            any = true;
        }
        return any && only_trailing_space(ss);
    }

    constexpr int MAX_PIECE_POINTS = 20;

    template <typename T> bool set(const std::string& value, T& target)
    {
        std::stringstream ss(value);
        T parsed{};
        ss >> parsed;
        if (ss.fail() || !only_trailing_space(ss))
            return false;
        target = parsed;
        return true;
    }

    template <> bool set(const std::string& value, int& target)
    {
        if (value.empty())
            return false;

        const char* first = value.data();
        const char* last  = first + value.size();
        int parsed = 0;
        auto [ptr, ec] = std::from_chars(first, last, parsed);
        while (ptr != last && std::isspace(static_cast<unsigned char>(*ptr)))
            ptr++;
        if (ec != std::errc() || ptr != last)
            return false;
        target = parsed;
        return true;
    }

    template <> bool set(const std::string& value, std::vector<int>& target)
    {
        std::stringstream ss(value);
        int i;
        std::vector<int> parsed;
        while (ss >> i)
            parsed.push_back(i);
        if (!ss.eof())
            return false;
        target = std::move(parsed);
        return true;
    }

    template <> bool set(const std::string& value, Rank& target) {
        int rank = 0;
        if (!parse_rank_index(value, rank))
            return false;
        Rank parsed = Rank(rank);
        if (parsed < RANK_1 || parsed > RANK_MAX)
            return false;
        target = parsed;
        return true;
    }

    template <> bool set(const std::string& value, File& target) {
        int file = 0;
        if (!parse_file_index(value, file))
            return false;
        File parsed = File(file);
        if (parsed < FILE_A || parsed > FILE_MAX)
            return false;
        target = parsed;
        return true;
    }

    template <> bool set(const std::string& value, std::string& target) {
        target = value;
        return true;
    }

    template <> bool set(const std::string& value, bool& target) {
        static constexpr auto values = std::array{
            std::pair{"true", true},
            std::pair{"false", false},
        };
        return parse_named_value(value, target, values);
    }

    template <> bool set(const std::string& value, Value& target) {
        static constexpr auto values = std::array{
            std::pair{"win", VALUE_MATE},
            std::pair{"loss", -VALUE_MATE},
            std::pair{"draw", VALUE_DRAW},
            std::pair{"none", VALUE_NONE},
        };
        return parse_named_value(value, target, values);
    }

    template <> bool set(const std::string& value, CapturingRule& target) {
        static constexpr auto values = std::array{
            std::pair{"out", MOVE_OUT},
            std::pair{"hand", HAND},
            std::pair{"prison", PRISON},
        };
        return parse_named_value(value, target, values);
    }

    template <> bool set(const std::string& value, PushFirstColor& target) {
        static constexpr auto values = std::array{
            std::pair{"us", PUSH_US},
            std::pair{"them", PUSH_THEM},
            std::pair{"either", PUSH_EITHER},
        };
        return parse_named_value(value, target, values);
    }

    template <> bool set(const std::string& value, PushRemoval& target) {
        static constexpr auto values = std::array{
            std::pair{"none", PUSH_REMOVE_NONE},
            std::pair{"shove", PUSH_REMOVE_SHOVE},
        };
        return parse_named_value(value, target, values);
    }

    template <> bool set(const std::string& value, MaterialCounting& target) {
        static constexpr auto values = std::array{
            std::pair{"janggi", JANGGI_MATERIAL},
            std::pair{"unweighted", UNWEIGHTED_MATERIAL},
            std::pair{"whitedrawodds", WHITE_DRAW_ODDS},
            std::pair{"blackdrawodds", BLACK_DRAW_ODDS},
            std::pair{"connectn", CONNECT_N_COUNT},
            std::pair{"none", NO_MATERIAL_COUNTING},
        };
        return parse_named_value(value, target, values);
    }

    template <> bool set(const std::string& value, CountingRule& target) {
        static constexpr auto values = std::array{
            std::pair{"makruk", MAKRUK_COUNTING},
            std::pair{"cambodian", CAMBODIAN_COUNTING},
            std::pair{"asean", ASEAN_COUNTING},
            std::pair{"none", NO_COUNTING},
        };
        return parse_named_value(value, target, values);
    }

    template <> bool set(const std::string& value, ChasingRule& target) {
        static constexpr auto values = std::array{
            std::pair{"axf", AXF_CHASING},
            std::pair{"none", NO_CHASING},
        };
        return parse_named_value(value, target, values);
    }

    template <> bool set(const std::string& value, EnclosingRule& target) {
        static constexpr auto values = std::array{
            std::pair{"reversi", REVERSI},
            std::pair{"ataxx", ATAXX},
            std::pair{"quadwrangle", QUADWRANGLE},
            std::pair{"snort", SNORT},
            std::pair{"anyside", ANYSIDE},
            std::pair{"top", TOP},
            std::pair{"none", NO_ENCLOSING},
        };
        return parse_named_value(value, target, values);
    }

    template <> bool set(const std::string& value, WallingRule& target) {
        static constexpr auto values = std::array{
            std::pair{"arrow", ARROW},
            std::pair{"duck", DUCK},
            std::pair{"edge", EDGE},
            std::pair{"past", PAST},
            std::pair{"static", STATIC},
            std::pair{"none", NO_WALLING},
        };
        return parse_named_value(value, target, values);
    }

    template <> bool set(const std::string& value, ColorChangeTrigger& target) {
        static constexpr auto values = std::array{
            std::pair{"capture", ColorChangeTrigger::ON_CAPTURE},
            std::pair{"non-capture", ColorChangeTrigger::ON_NON_CAPTURE},
            std::pair{"always", ColorChangeTrigger::ALWAYS},
            std::pair{"never", ColorChangeTrigger::NEVER},
            std::pair{"none", ColorChangeTrigger::NEVER},
        };
        return parse_named_value(value, target, values);
    }

    template <> bool set(const std::string& value, EnPassantPassedSquares& target) {
        static constexpr auto values = std::array{
            std::pair{"first", EnPassantPassedSquares::FIRST},
            std::pair{"last", EnPassantPassedSquares::LAST},
            std::pair{"all", EnPassantPassedSquares::ALL},
        };
        return parse_named_value(value, target, values);
    }

    template <> bool set(const std::string& value, PointsRule& target) {
        static constexpr auto values = std::array{
            std::pair{"us", POINTS_US},
            std::pair{"them", POINTS_THEM},
            std::pair{"owner", POINTS_OWNER},
            std::pair{"non-owner", POINTS_NON_OWNER},
            std::pair{"none", POINTS_NONE},
        };
        return parse_named_value(value, target, values);
    }

    template <> bool set(const std::string& value, TransferSide& target) {
        static constexpr auto values = std::array{
            std::pair{"us", TRANSFER_US},
            std::pair{"them", TRANSFER_THEM},
            std::pair{"owner", TRANSFER_OWNER},
            std::pair{"non-owner", TRANSFER_NON_OWNER},
        };
        return parse_named_value(value, target, values);
    }

    template <> bool set(const std::string& value, Bitboard& target) {
        std::string symbol;
        std::stringstream ss(value);
        Bitboard parsed = 0;
        while (!ss.eof() && ss >> symbol && symbol != "-")
        {
            if (symbol.back() == '*') {
                if (std::isalpha(static_cast<unsigned char>(symbol[0])) && symbol.length() == 2) {
                    char file = std::tolower(static_cast<unsigned char>(symbol[0]));
                    if (File(file - 'a') > FILE_MAX) return false;
                    parsed |= file_bb(File(file - 'a'));
                } else {
                    return false;
                }
            } else if (symbol[0] == '*') {
                int rank = 0;
                if (!parse_positive_int(symbol.substr(1), rank) || Rank(rank - 1) > RANK_MAX)
                    return false;
                parsed |= rank_bb(Rank(rank - 1));
            } else if (std::isalpha(static_cast<unsigned char>(symbol[0])) && symbol.length() > 1) {
                char file = std::tolower(static_cast<unsigned char>(symbol[0]));
                int rank = 0;
                if (!parse_positive_int(symbol.substr(1), rank)
                    || Rank(rank - 1) > RANK_MAX
                    || File(file - 'a') > FILE_MAX)
                    return false;
                parsed |= square_bb(make_square(File(file - 'a'), Rank(rank - 1)));
            } else {
                return false;
            }
        }
        if (ss.fail() || !only_trailing_space(ss))
            return false;
        target = parsed;
        return true;
    }

    template <> bool set(const std::string& value, PieceTypeBitboardGroup& target) {
        // Try parsing as Bitboard first for backward compatibility
        Bitboard b;
        if (set(value, b)) {
            target = PieceTypeBitboardGroup(b);
            return true;
        }
        size_t i;
        int ParserState = -1;
        int RankNum = 0;
        int FileNum = 0;
        char PieceChar = 0;
        Bitboard board = 0x00;
        PieceTypeBitboardGroup parsedTarget = target;
        // String parser using state machine
        for (i = 0; i < value.length(); i++)
        {
            const char ch = value.at(i);
            if (ch == ' ')
            {
                continue;
            }
            if (ParserState == -1)  // Initial state, if "-" exists here then it means a null value. e.g. promotionRegion = - means no promotion region
            {
                if (ch == '-')
                {
                    for (size_t j = i + 1; j < value.length(); ++j)
                        if (!std::isspace(static_cast<unsigned char>(value[j])))
                            return false;
                    parsedTarget = PieceTypeBitboardGroup();
                    target = parsedTarget;
                    return true;
                }
                ParserState = 0;
            }
            if (ParserState == 0)  // Find piece type character
            {
                if ((ch >= 'A' && ch <= 'Z') || ch == '*')
                {
                    PieceChar = ch;
                    ParserState = 1;
                }
                else
                {
                    std::cerr << "At char " << i << " of PieceTypeBitboardGroup declaration: Illegal piece type character: " << ch << std::endl;
                    return false;
                }
            }
            else if (ParserState == 1)  // Find "("
            {
                if (ch == '(')
                {
                    ParserState = 2;
                }
                else
                {
                    std::cerr << "At char " << i << " of PieceTypeBitboardGroup declaration: Expect \"(\". Actual: " << ch << std::endl;
                    return false;
                }
            }
            else if (ParserState == 2)  //Find file
            {
                if (ch >= 'a' && ch <= 'z')
                {
                    FileNum = ch - 'a';
                    ParserState = 3;
                }
                else if (ch == '*')
                {
                    FileNum = -1;
                    ParserState = 3;
                }
                else
                {
                    std::cerr << "At char " << i << " of PieceTypeBitboardGroup declaration: Illegal file character: " << ch << std::endl;
                    return false;
                }
            }
            else if (ParserState == 3)  //Find rank and terminator "," or ")"
            {
                if (ch == '*')
                {
                    RankNum = -1;
                }
                else if (ch >= '0' && ch <= '9' && RankNum >= 0)
                {
                    if (RankNum > (std::numeric_limits<int>::max() - (ch - '0')) / 10)
                    {
                        std::cerr << "At char " << i << " of PieceTypeBitboardGroup declaration: Rank number overflow." << std::endl;
                        return false;
                    }
                    RankNum = RankNum * 10 + (ch - '0');
                }
                else if (ch == ',' || ch == ')')
                {
                    if (RankNum == 0)  // Here if RankNum==0 then it means either user declared a 0 as rank, or no rank number declared at all
                    {
                        std::cerr << "At char " << i << " of PieceTypeBitboardGroup declaration: Illegal rank number: " << RankNum << std::endl;
                        return false;
                    }
                    if (RankNum > 0)  //When RankNum==-1, it means a whole File.
                    {
                        RankNum--;
                    }
                    if (RankNum < -1 || RankNum > RANK_MAX)
                    {
                        std::cerr << "At char " << i << " of PieceTypeBitboardGroup declaration: Max rank number exceeds. Max: " << RANK_MAX << "; Actual: " << RankNum << std::endl;
                        return false;
                    }
                    else if (FileNum < -1 || FileNum > FILE_MAX)
                    {
                        std::cerr << "At char " << i << " of PieceTypeBitboardGroup declaration: Max file number exceeds. Max: " << FILE_MAX << "; Actual: " << FileNum << std::endl;
                        return false;
                    }
                    if (RankNum == -1 && FileNum == -1)
                    {
                        board = Bitboard(-1);
                    }
                    else if (FileNum == -1)
                    {
                        board |= rank_bb(Rank(RankNum));
                    }
                    else if (RankNum == -1)
                    {
                        board |= file_bb(File(FileNum));
                    }
                    else
                    {
                        board |= square_bb(make_square(File(FileNum), Rank(RankNum)));
                    }
                    if (ch == ')')
                    {
                        // Repeated piece clauses (e.g. "P(a8);P(h8)") are additive.
                        parsedTarget.set(PieceChar, parsedTarget.boardOfPiece(PieceChar) | board);
                        ParserState = 4;
                    }
                    else
                    {
                        RankNum = 0;
                        FileNum = 0;
                        ParserState = 2;
                    }
                }
                else
                {
                    std::cerr << "At char " << i << " of PieceTypeBitboardGroup declaration: Illegal rank character: " << ch << std::endl;
                    return false;
                }
            }
            else if (ParserState == 4)  // Find ";"
            {
                if (ch == ';')
                {
                    ParserState = 0;
                    RankNum = 0;
                    FileNum = 0;
                    PieceChar = 0;
                    board = 0x00;
                }
                else
                {
                    std::cerr << "At char " << i << " of PieceTypeBitboardGroup declaration: Expects \";\"." << std::endl;
                    return false;
                }
            }
        }
        if (ParserState != 0 && ParserState != 4)
        {
            std::cerr << "At char " << i << " of PieceTypeBitboardGroup declaration: Unterminated expression." << std::endl;
            return false;
        }
        target = parsedTarget;
        return true;
    }


    template <> bool set(const std::string& value, CastlingRights& target) {
        char c;
        CastlingRights castlingRight;
        std::stringstream ss(value);
        CastlingRights parsed = NO_CASTLING;
        bool valid = true;
        while (ss >> c && c != '-')
        {
            castlingRight =  c == 'K' ? WHITE_OO
                           : c == 'Q' ? WHITE_OOO
                           : c == 'k' ? BLACK_OO
                           : c == 'q' ? BLACK_OOO
                           : NO_CASTLING;
            if (castlingRight)
                parsed = CastlingRights(parsed | castlingRight);
            else
            {
                valid = false;
                break;
            }
        }
        if (valid && only_trailing_space(ss))
            target = parsed;
        return valid && only_trailing_space(ss);
    }

    template <typename T> void set(PieceType pt, T& target) {
        target.insert(pt);
    }

    template <> void set(PieceType pt, PieceType& target) {
        target = pt;
    }

    template <> void set(PieceType pt, PieceSet& target) {
        target |= pt;
    }

    template <> void set(PieceType pt, FilePieceSetMap& target) {
        target |= pt;
    }

    bool parse_hostage_exchanges(Variant *v, const std::string &map, bool DoCheck) {
        std::stringstream groups(map);
        std::string group;
        PieceSet parsed[PIECE_TYPE_NB];
        std::copy(v->hostageExchange, v->hostageExchange + PIECE_TYPE_NB, parsed);
        bool sawGroup = false;
        while (std::getline(groups, group, ' '))
        {
            group = trim(group);
            if (group.empty())
                continue;
            sawGroup = true;
            auto [token, rest] = split_piece_entry(group);
            PieceType from = parse_piece_type_token(v, token);
            if (from == NO_PIECE_TYPE)
            {
                if (DoCheck)
                    std::cerr << "hostageExchange - Invalid piece type: " << token << std::endl;
                return false;
            }
            PieceSet mask = NO_PIECE_SET;
            std::string rhs = trim(rest);
            while (!rhs.empty())
            {
                std::string hostage = read_piece_token(rhs);
                if (hostage.empty())
                {
                    if (DoCheck)
                        std::cerr << "hostageExchange - Invalid hostage piece type in: " << group << std::endl;
                    return false;
                }
                PieceType pt = parse_piece_type_token(v, hostage);
                if (pt == NO_PIECE_TYPE)
                {
                    if (DoCheck)
                        std::cerr << "hostageExchange - Invalid hostage piece type: " << hostage << std::endl;
                    return false;
                }
                mask |= pt;
                rhs.erase(0, hostage.size());
                rhs = trim(rhs);
            }
            parsed[from] = mask;
        }
        if (!sawGroup)
        {
            if (DoCheck)
                std::cerr << "hostageExchange - Empty value is not allowed." << std::endl;
            return false;
        }
        std::copy(parsed, parsed + PIECE_TYPE_NB, v->hostageExchange);
        return true;
    }

    bool parse_file_piece_set_map(const std::string& value,
                                  const Variant* v,
                                  File maxFile,
                                  FilePieceSetMap& target,
                                  bool doCheck,
                                  const std::string& key) {
        std::stringstream ss(value);
        std::string token;
        FilePieceSetMap parsed = target;
        bool sawToken = false;

        while (true) {
            ss >> std::ws;
            if (ss.eof())
                break;

            if (!(ss >> token))
                break;
            sawToken = true;

            std::string fileToken = token;
            std::string pieceToken;
            size_t colon = token.find(':');
            if (colon != std::string::npos) {
                fileToken = token.substr(0, colon);
                pieceToken = token.substr(colon + 1);
            }

            int fileIdx = -1;
            bool isFallback = (fileToken == "*");
            if (!isFallback && (!parse_file_index(fileToken, fileIdx) || fileIdx < 0 || fileIdx > int(maxFile))) {
                if (doCheck)
                    std::cerr << key << " - Invalid file: " << fileToken << std::endl;
                return false;
            }

            if (colon == std::string::npos) {
                char sep = '\0';
                if (!(ss >> sep) || sep != ':') {
                    if (doCheck)
                        std::cerr << key << " - Expected ':' after file " << fileToken << std::endl;
                    return false;
                }
                if (!(ss >> pieceToken)) {
                    if (doCheck)
                        std::cerr << key << " - Missing piece set for file " << fileToken << std::endl;
                    return false;
                }
            } else if (pieceToken.empty()) {
                if (!(ss >> pieceToken)) {
                    if (doCheck)
                        std::cerr << key << " - Missing piece set for file " << fileToken << std::endl;
                    return false;
                }
            }

            if (pieceToken.empty()) {
                if (doCheck)
                    std::cerr << key << " - Missing piece set for file " << fileToken << std::endl;
                return false;
            }

            PieceSet pieces = NO_PIECE_SET;
            if (pieceToken != "-") {
                if (!parse_piece_set_token_string(pieceToken, v, pieces))
                    return false;
            }

            if (isFallback) parsed.fallback = pieces;
            else parsed.set(File(fileIdx), pieces);
        }

        if (!sawToken) {
            if (doCheck)
                std::cerr << key << " - Invalid value " << value << std::endl;
            return false;
        }

        target = parsed;
        return true;
    }

} // namespace

template <bool DoCheck>
template <bool Current, class T> bool VariantParser<DoCheck>::parse_attribute(const std::string& key, T& target) {
    const auto& it = config.find(key);
    if (it != config.end())
    {
        bool valid = set(it->second, target);
        if (DoCheck && !Current)
            std::cerr << key << " - Deprecated option might be removed in future version." << std::endl;
        if (DoCheck && !valid)
        {
            std::string typeName =  std::is_same<T, int>() ? "int"
                                  : std::is_same<T, Rank>() ? "Rank"
                                  : std::is_same<T, File>() ? "File"
                                  : std::is_same<T, bool>() ? "bool"
                                  : std::is_same<T, Value>() ? "Value"
                                  : std::is_same<T, MaterialCounting>() ? "MaterialCounting"
                                  : std::is_same<T, CountingRule>() ? "CountingRule"
                                  : std::is_same<T, ChasingRule>() ? "ChasingRule"
                                  : std::is_same<T, CapturingRule>() ? "CapturingRule"
                                  : std::is_same<T, EnclosingRule>() ? "EnclosingRule"
                                  : std::is_same<T, Bitboard>() ? "Bitboard"
                                  : std::is_same<T, PieceTypeBitboardGroup>() ? "PieceTypeBitboardGroup"
                                  : std::is_same<T, CastlingRights>() ? "CastlingRights"
                                  : std::is_same<T, ColorChangeTrigger>() ? "ColorChangeTrigger"
                                  : std::is_same<T, EnPassantPassedSquares>() ? "EnPassantPassedSquares"
                                  : std::is_same<T, WallingRule>() ? "WallingRule"
                                  : std::is_same<T, std::vector<int>>() ? "vector<int>"
                                  : typeid(T).name();
            std::cerr << key << " - Invalid value " << it->second << " for type " << typeName << std::endl;
        }
        if (!valid)
            parseHadError = true;
        return valid;
    }
    return false;
}

template <bool DoCheck>
template <bool Current, class T> bool VariantParser<DoCheck>::parse_attribute(const std::string& key, T& target, const Variant* v) {
    const auto& it = config.find(key);
    if (it != config.end())
    {
        if (DoCheck && !Current)
            std::cerr << key << " - Deprecated option might be removed in future version." << std::endl;

        if constexpr (std::is_same_v<T, PieceSet>)
        {
            PieceSet parsedTarget = NO_PIECE_SET;
            if (parse_piece_set_token_string(it->second, v, parsedTarget))
            {
                target = parsedTarget;
                return true;
            }
            if (DoCheck)
                std::cerr << key << " - Invalid piece type: " << it->second << std::endl;
            parseHadError = true;
            return false;
        }

        if constexpr (std::is_same_v<T, PieceType>)
        {
            if (trim(it->second) == "-")
            {
                target = NO_PIECE_TYPE;
                return true;
            }
        }

        if constexpr (std::is_same_v<T, FilePieceSetMap>)
        {
            if (it->second.find(':') == std::string::npos)
            {
                PieceSet globalSet = NO_PIECE_SET;
                if (parse_piece_set_token_string(it->second, v, globalSet))
                {
                    target = FilePieceSetMap(globalSet);
                    return true;
                }
            }
            else
            {
                FilePieceSetMap parsedTarget = Current ? target : FilePieceSetMap();
                if (parse_file_piece_set_map(it->second, v, v->maxFile, parsedTarget, DoCheck, key))
                {
                    target = parsedTarget;
                    return true;
                }
                parseHadError = true;
                return false;
            }
        }

        T parsedTarget = Current ? target : T();
        std::string token;
        std::stringstream ss(it->second);
        bool sawToken = false;
        bool valid = true;
        while (ss >> token && token != "-")
        {
            sawToken = true;
            PieceType pt = token == "*" ? ALL_PIECES : parse_piece_type_token(v, token);
            if (pt == NO_PIECE_TYPE)
            {
                valid = false;
                break;
            }
            set(pt, parsedTarget);
        }
        if (DoCheck && !valid && token != "-")
            std::cerr << key << " - Invalid piece type: " << token << std::endl;
        else if ((sawToken || token == "-") && !only_trailing_space(ss))
        {
            if (DoCheck)
                std::cerr << key << " - Invalid trailing characters." << std::endl;
            parseHadError = true;
            return false;
        }

        if ((sawToken && valid) || token == "-")
        {
            target = parsedTarget;
            return true;
        }
        parseHadError = true;
        return false;
    }
    return false;
}

template <bool DoCheck>
template <typename T>
bool VariantParser<DoCheck>::require_attribute(bool enabled, const std::string& key, T& target) {
    if (!enabled)
        return true;
    if (parse_attribute(key, target))
        return true;
    if (DoCheck)
        std::cerr << "Syntax error in " << key << " or missing " << key << " definition." << std::endl;
    return false;
}

template <bool DoCheck>
template <typename T, typename U>
bool VariantParser<DoCheck>::require_attributes(bool enabled,
                                                const std::string& key1, T& target1,
                                                const std::string& key2, U& target2) {
    return require_attribute(enabled, key1, target1)
        && require_attribute(enabled, key2, target2);
}

template <bool DoCheck>
template <typename T>
void VariantParser<DoCheck>::parse_both_colors(const std::string& key, T& target) {
    parse_attribute(key, target[WHITE]);
    parse_attribute(key, target[BLACK]);
}

template <bool DoCheck>
template <typename T>
void VariantParser<DoCheck>::parse_both_colors_piece(const std::string& key, T& target, const Variant* v) {
    parse_attribute(key, target[WHITE], v);
    parse_attribute(key, target[BLACK], v);
}

template <bool DoCheck>
template <typename T>
void VariantParser<DoCheck>::parse_both_colors_with_overrides(const std::string& key, T& target) {
    parse_both_colors(key, target);
    parse_attribute(key + "White", target[WHITE]);
    parse_attribute(key + "Black", target[BLACK]);
}

template <bool DoCheck>
template <typename T>
void VariantParser<DoCheck>::parse_both_colors_with_overrides_piece(const std::string& key, T& target, const Variant* v) {
    parse_both_colors_piece(key, target, v);
    parse_attribute(key + "White", target[WHITE], v);
    parse_attribute(key + "Black", target[BLACK], v);
}

template <bool DoCheck>
template <typename T>
void VariantParser<DoCheck>::parse_color_setting(const std::string& key, ColorSetting<T>& target) {
    parse_color_triplet(config, key, [&](const std::string& option, Color color) {
        T parsed = color == WHITE ? target.byColor[WHITE] : color == BLACK ? target.byColor[BLACK] : target.global;
        if (parse_attribute(option, parsed))
        {
            if (color == WHITE)
                target.set_color(WHITE, parsed);
            else if (color == BLACK)
                target.set_color(BLACK, parsed);
            else
                target.set_global(parsed);
        }
    });
}

template <bool DoCheck>
template <typename T>
bool VariantParser<DoCheck>::parse_color_setting_piece(const std::string& key, ColorSetting<T>& target, const Variant* v) {
    bool ok = true;
    parse_color_triplet(config, key, [&](const std::string& option, Color color) {
        T parsed = color == WHITE ? target.byColor[WHITE] : color == BLACK ? target.byColor[BLACK] : target.global;
        if (parse_attribute(option, parsed, v))
        {
            if (color == WHITE)
                target.set_color(WHITE, parsed);
            else if (color == BLACK)
                target.set_color(BLACK, parsed);
            else
                target.set_global(parsed);
        }
        else
        {
            if (DoCheck)
                std::cerr << option << " - Invalid syntax." << std::endl;
            ok = false;
        }
    });
    return ok;
}

template <bool DoCheck>
bool VariantParser<DoCheck>::parse_color_setting_first_piece(const std::string& key, ColorSetting<PieceType>& target, const Variant* v) {
    bool ok = true;
    parse_color_triplet(config, key, [&](const std::string& option, Color color) {
        PieceType parsed = color == WHITE ? target.byColor[WHITE] : color == BLACK ? target.byColor[BLACK] : target.global;
        const auto& it = config.find(option);
        if (it == config.end())
            return;
        if (trim(it->second) == "-")
        {
            parsed = NO_PIECE_TYPE;
            if (color == WHITE)
                target.set_color(WHITE, parsed);
            else if (color == BLACK)
                target.set_color(BLACK, parsed);
            else
                target.set_global(parsed);
            return;
        }
        if (parse_first_piece_type_token(v, it->second, parsed))
        {
            if (color == WHITE)
                target.set_color(WHITE, parsed);
            else if (color == BLACK)
                target.set_color(BLACK, parsed);
            else
                target.set_global(parsed);
        }
        else
        {
            if (DoCheck)
                std::cerr << option << " - Invalid syntax." << std::endl;
            ok = false;
        }
    });
    return ok;
}

template <bool DoCheck>
bool VariantParser<DoCheck>::parse_piece_types(Variant* v) {
    for (PieceType pt = PAWN; pt <= KING; ++pt)
    {
        if (pt == CUSTOM_PIECES_ROYAL)
            // reserved custom royal/king slot
            continue;

        // piece char
        std::string name = piece_name(pt);

        const auto& keyValue = config.find(name);
        if (keyValue != config.end() && !keyValue->second.empty())
        {
            auto [token, rest] = split_piece_entry(keyValue->second);
            if (!token.empty())
                v->add_piece(pt, token);
            else
            {
                if (keyValue->second.at(0) == '-')
                    v->remove_piece(pt);
                else
                {
                    if (DoCheck)
                        std::cerr << name << " - Invalid letter: " << keyValue->second.at(0) << std::endl;
                    return false;
                }
            }
            // betza
            if (is_custom(pt))
            {
                if (!rest.empty())
                {
                    v->customPiece[pt - CUSTOM_PIECES] = rest;
                    // Is there an en passant flag in the Betza notation?
                    if (v->customPiece[pt - CUSTOM_PIECES].find('e') != std::string::npos)
                    {
                        v->enPassantTypes[WHITE] |= piece_set(pt);
                        v->enPassantTypes[BLACK] |= piece_set(pt);
                    }
                }
                else if (DoCheck)
                    std::cerr << name << " - Missing Betza move notation" << std::endl;
            }
            else if (pt != KING && !rest.empty())
            {
                if (DoCheck)
                    std::cerr << name << " only supports a piece letter here. Use customPieceN = "
                              << keyValue->second << " and remap " << name << " to that letter instead." << std::endl;
                return false;
            }
            else if (pt == KING)
            {
                if (!rest.empty())
                {
                    // custom royal piece
                    v->add_piece(CUSTOM_PIECES_ROYAL, token);
                    v->customPiece[CUSTOM_PIECES_ROYAL - CUSTOM_PIECES] = rest;
                    v->kingType = CUSTOM_PIECES_ROYAL;
                    v->castlingKingPiece[WHITE] = v->castlingKingPiece[BLACK] = CUSTOM_PIECES_ROYAL;
                }
                else
                    v->kingType = KING;
            }
        }
        // mobility region
        std::string capitalizedPiece = name;
        capitalizedPiece[0] = std::toupper(static_cast<unsigned char>(capitalizedPiece[0]));
        for (Color c : {WHITE, BLACK})
        {
            std::string color = c == WHITE ? "White" : "Black";
            parse_attribute("mobilityRegion" + color + capitalizedPiece, v->mobilityRegion[c][pt]);
        }
    }
    return true;
}

template <bool DoCheck>
bool VariantParser<DoCheck>::parse_piece_values(Variant* v) {
    for (Phase phase : {MG, EG})
    {
        const std::string optionName = phase == MG ? "pieceValueMg" : "pieceValueEg";
        const auto& pv = config.find(optionName);
        if (pv != config.end())
        {
            std::string entry;
            PieceType pt = NO_PIECE_TYPE;
            bool parseError = false;
            int parsedValue = 0;
            int parsed[PIECE_TYPE_NB];
            std::copy(std::begin(v->pieceValue[phase]), std::end(v->pieceValue[phase]), std::begin(parsed));
            std::stringstream ss(pv->second);
            while (ss >> entry)
            {
                auto [token, rawValue] = split_piece_entry(entry);
                pt = parse_piece_type_token(v, token);
                if (pt == NO_PIECE_TYPE || rawValue.empty())
                {
                    parseError = true;
                    break;
                }
                if (!set(rawValue, parsedValue))
                {
                    parseError = true;
                    break;
                }
                parsed[pt] = parsedValue;
            }
            if (DoCheck && parseError)
                std::cerr << optionName << " - Invalid syntax." << std::endl;
            if (!parseError)
                std::copy(std::begin(parsed), std::end(parsed), std::begin(v->pieceValue[phase]));
        }
    }

    // piece points (for games of points, not evaluation)
    const auto& pv = config.find("piecePoints");
    if (pv != config.end())
    {
        if (!parse_piece_int_map_option("piecePoints", pv->second, v, v->piecePoints, true, DoCheck, [&](int (&parsed)[PIECE_TYPE_NB]) {
            for (PieceType pt = PAWN; pt < PIECE_TYPE_NB; ++pt)
            {
                int points = parsed[pt];
                if (points < 0)
                    points = 0;
                if (points > MAX_PIECE_POINTS)
                    points = MAX_PIECE_POINTS;
                parsed[pt] = points;
            }
            return true;
        }))
            return false;
    }
    return true;
}

template <bool DoCheck>
bool VariantParser<DoCheck>::parse_legacy_attributes(Variant* v) {
    // Parse deprecated values for backwards compatibility
    Rank promotionRank = RANK_8;
    if (parse_attribute<false>("promotionRank", promotionRank))
    {
        for (Color c : {WHITE, BLACK})
            v->promotionRegion[c] = zone_bb(c, promotionRank, v->maxRank);
    }
    Rank doubleStepRank = RANK_2;
    Rank doubleStepRankMin = RANK_2;
    if (   parse_attribute<false>("doubleStepRank", doubleStepRank)
        || parse_attribute<false>("doubleStepRankMin", doubleStepRankMin))
    {
        for (Color c : {WHITE, BLACK})
            v->doubleStepRegion[c] =   zone_bb(c, doubleStepRankMin, v->maxRank)
                                    & ~forward_ranks_bb(c, relative_rank(c, doubleStepRank, v->maxRank));
    }
    if (!parse_named_color_pair(
            config, "whiteFlag", "blackFlag",
            v->flagRegion[WHITE], v->flagRegion[BLACK], DoCheck,
            [&](const std::string&, const std::string& raw, Bitboard& target) { return set(raw, target); }))
        return false;
    parse_attribute<false>("castlingRookPiece", v->castlingRookPieces[WHITE], v);
    parse_attribute<false>("castlingRookPiece", v->castlingRookPieces[BLACK], v);
    if (!parse_named_color_pair(
            config, "whiteDropRegion", "blackDropRegion",
            v->dropRegion[WHITE], v->dropRegion[BLACK], DoCheck,
            [&](const std::string&, const std::string& raw, PieceTypeBitboardGroup& target) { return set(raw, target); }))
        return false;

    bool dropOnTop = false;
    parse_attribute<false>("dropOnTop", dropOnTop);
    if (dropOnTop) v->enclosingDrop=TOP;

    // Parse aliases
    if (!parse_color_setting_first_piece("pawnTypes", v->mainPromotionPawnType, v)) return false;
    if (!parse_color_setting_piece("pawnTypes", v->promotionPawnTypes, v)) return false;
    if (!parse_color_setting_piece("pawnTypes", v->enPassantTypes, v)) return false;
    if (!parse_color_setting_piece("pawnTypes", v->nMoveRuleTypes, v)) return false;

    bool blastDiagonals = true;
    bool blastOrthogonals = true;
    bool blastCenter = true;
    bool hasD = parse_attribute("blastDiagonals", blastDiagonals);
    bool hasO = parse_attribute("blastOrthogonals", blastOrthogonals);
    bool hasC = parse_attribute("blastCenter", blastCenter);
    if (hasD || hasO || hasC)
    {
        v->blastPattern = "";
        if (blastDiagonals && blastOrthogonals) v->blastPattern += "K";
        else if (blastDiagonals) v->blastPattern += "F";
        else if (blastOrthogonals) v->blastPattern += "W";
        if (blastCenter) v->blastPattern += "*";
    }

    return true;
}

template <bool DoCheck>
bool VariantParser<DoCheck>::parse_official_options(Variant* v) {
    // Parse the official config options
    parse_attribute("variantTemplate", v->variantTemplate);
    parse_attribute("pieceToCharTable", v->pieceToCharTable);
    parse_attribute("pocketSize", v->pocketSize);
    parse_attribute("chess960", v->chess960);
    parse_attribute("twoBoards", v->twoBoards);
    parse_attribute("hexBoard", v->hexBoard);
    parse_attribute("cylindrical", v->cylindrical);
    parse_attribute("toroidal", v->toroidal);
    parse_attribute("startFen", v->startFen);
    parse_color_setting("promotionRegion", v->promotionRegion);
    parse_color_setting("mandatoryPromotionRegion", v->mandatoryPromotionRegion);
    // Take the first promotionPawnTypes as the main promotionPawnType
    if (!parse_color_setting_first_piece("promotionPawnTypes", v->mainPromotionPawnType, v)) return false;
    if (!parse_color_setting_piece("promotionPawnTypes", v->promotionPawnTypes, v)) return false;
    if (!parse_color_setting_piece("promotionPieceTypes", v->promotionPieceTypes, v)) return false;
    parse_attribute("sittuyinPromotion", v->sittuyinPromotion);
    parse_attribute("promotionSteal", v->promotionSteal);
    parse_attribute("promotionRequireInHand", v->promotionRequireInHand);
    parse_attribute("promotionConsumeInHand", v->promotionConsumeInHand);
    // promotion limit
    const auto& it_prom_limit = config.find("promotionLimit");
    if (it_prom_limit != config.end())
    {
        if (!parse_piece_int_map_option("promotionLimit", it_prom_limit->second, v, v->promotionLimit, true, DoCheck, [&](int (&parsed)[PIECE_TYPE_NB]) {
            for (PieceType pt = PAWN; pt < PIECE_TYPE_NB; ++pt)
            {
                if (parsed[pt] < 0)
                {
                    if (DoCheck)
                        std::cerr << "promotionLimit - Invalid negative value." << std::endl;
                    return false;
                }
            }
            return true;
        }))
            return false;
    }
    // promoted piece types
    const auto& it_prom_pt = config.find("promotedPieceType");
    if (it_prom_pt != config.end())
    {
        if (!parse_piece_type_map(it_prom_pt->second, v, v->promotedPieceType, true))
        {
            if (DoCheck)
                std::cerr << "promotedPieceType - Invalid syntax." << std::endl;
            return false;
        }
    }
    const auto& it_move_morph_pt = config.find("moveMorphPieceType");
    if (it_move_morph_pt != config.end())
    {
        if (!parse_piece_type_map(it_move_morph_pt->second, v, v->moveMorphPieceType, true))
        {
            if (DoCheck)
                std::cerr << "moveMorphPieceType - Invalid syntax." << std::endl;
            return false;
        }
    }
    const auto& it_gate_after = config.find("gatingPieceAfter");
    if (it_gate_after != config.end())
    {
        std::array<PieceType, PIECE_TYPE_NB> parsed{};
        if (!parse_piece_type_map(it_gate_after->second, v, parsed.data(), true))
        {
            if (DoCheck)
                std::cerr << "gatingPieceAfter - Invalid syntax." << std::endl;
            return false;
        }
        v->gatingPieceAfter.set_global(parsed);
    }
    const auto& it_gate_after_w = config.find("gatingPieceAfterWhite");
    if (it_gate_after_w != config.end())
    {
        std::array<PieceType, PIECE_TYPE_NB> parsed{};
        if (!parse_piece_type_map(it_gate_after_w->second, v, parsed.data(), true))
        {
            if (DoCheck)
                std::cerr << "gatingPieceAfterWhite - Invalid syntax." << std::endl;
            return false;
        }
        v->gatingPieceAfter.set_color(WHITE, parsed);
    }
    const auto& it_gate_after_b = config.find("gatingPieceAfterBlack");
    if (it_gate_after_b != config.end())
    {
        std::array<PieceType, PIECE_TYPE_NB> parsed{};
        if (!parse_piece_type_map(it_gate_after_b->second, v, parsed.data(), true))
        {
            if (DoCheck)
                std::cerr << "gatingPieceAfterBlack - Invalid syntax." << std::endl;
            return false;
        }
        v->gatingPieceAfter.set_color(BLACK, parsed);
    }
    const auto& it_first_move_pt = config.find("firstMovePieceTypes");
    if (it_first_move_pt != config.end())
    {
        if (!parse_piece_type_map(it_first_move_pt->second, v, v->firstMovePieceType, true))
        {
            if (DoCheck)
                std::cerr << "firstMovePieceTypes - Invalid syntax." << std::endl;
            return false;
        }
    }
    const auto& it_drop_piece_types = config.find("dropPieceTypes");
    if (it_drop_piece_types != config.end())
    {
        if (!parse_drop_piece_type_map(it_drop_piece_types->second, v, v->dropPieceTypes))
        {
            if (DoCheck)
                std::cerr << "dropPieceTypes - Invalid syntax." << std::endl;
            return false;
        }
    }
    // priority drops
    const auto& it_pr_drop = config.find("priorityDropTypes");
    if (it_pr_drop != config.end())
    {
        PieceSet parsedPriorityDrops = v->isPriorityDrop;
        bool sawToken = false;
        std::stringstream ss(it_pr_drop->second);
        std::string token;
        while (ss >> token)
        {
            sawToken = true;
            if (token == "-")
                break;
            PieceType pt = parse_piece_type_token(v, token);
            if (pt == NO_PIECE_TYPE)
            {
                if (DoCheck)
                std::cerr << "priorityDropTypes - Invalid piece type: " << token << std::endl;
                return false;
            }
            parsedPriorityDrops |= piece_set(pt);
        }
        if (sawToken && token != "-" && !only_trailing_space(ss))
        {
            if (DoCheck)
                std::cerr << "priorityDropTypes - Invalid trailing characters." << std::endl;
            return false;
        }
        else if (sawToken)
            v->isPriorityDrop = parsedPriorityDrops;
    }
    parse_attribute("piecePromotionOnCapture", v->piecePromotionOnCapture);
    parse_color_setting("mandatoryPawnPromotion", v->mandatoryPawnPromotion);
    parse_color_setting("mandatoryPiecePromotion", v->mandatoryPiecePromotion);
    parse_attribute("pieceDemotion", v->pieceDemotion);
    parse_attribute("blastOnCapture", v->blastOnCapture);
    parse_attribute("blastOnMove", v->blastOnMove);
    parse_attribute("blastOnSelfDestruct", v->blastOnSelfDestruct);
    parse_attribute("selfDestructTypes", v->selfDestructTypes, v);
    parse_attribute("blastPromotion", v->blastPromotion);
    parse_attribute("blastPattern", v->blastPattern);
    parse_attribute("blastPassiveTypes", v->blastPassiveTypes, v);
    parse_attribute("blastImmuneTypes", v->blastImmuneTypes, v);
    parse_attribute("mutuallyImmuneTypes", v->mutuallyImmuneTypes, v);
    parse_attribute("deathOnCaptureTypes", v->deathOnCaptureTypes, v);
    parse_attribute("mutuallyHopIllegalTypes", v->mutuallyHopIllegalTypes, v);
    auto parse_capture_map = [&](const std::string& key, bool allow) {
        const auto& it = config.find(key);
        if (it == config.end())
            return true;

        std::string entry;
        std::stringstream ss(it->second);
        PieceSet parsed[PIECE_TYPE_NB];
        std::copy(v->captureForbidden, v->captureForbidden + PIECE_TYPE_NB, parsed);
        while (ss >> entry) {
            size_t sep = entry.find(':');
            if (sep == std::string::npos || sep == 0 || sep + 1 >= entry.size()) {
                if (DoCheck)
                    std::cerr << key << " - Invalid mapping token: " << entry << std::endl;
                return false;
            }

            std::string attackers = entry.substr(0, sep);
            std::string targets = entry.substr(sep + 1);

            PieceSet attackerSet = NO_PIECE_SET;
            if (!parse_piece_set_token_string(attackers, v, attackerSet, true, false))
            {
                if (DoCheck)
                    std::cerr << key << " - Invalid attacker piece type list: " << attackers << std::endl;
                return false;
            }

            PieceSet targetSet = NO_PIECE_SET;
            if (!parse_piece_set_token_string(targets, v, targetSet, true, true))
            {
                if (DoCheck)
                    std::cerr << key << " - Invalid target piece type list: " << targets << std::endl;
                return false;
            }

            if (!attackerSet || !targetSet)
                return false;

            for (PieceSet ps = attackerSet; ps; ) {
                PieceType attacker = pop_lsb(ps);
                if (allow)
                    parsed[attacker] &= ~targetSet;
                else
                    parsed[attacker] |= targetSet;
            }
        }
        std::copy(parsed, parsed + PIECE_TYPE_NB, v->captureForbidden);
        return true;
    };
    if (!parse_capture_map("captureForbidden", false))
        return false;
    if (!parse_capture_map("captureAllowed", true))
        return false;
    parse_attribute("petrifyOnCaptureTypes", v->petrifyOnCaptureTypes, v);
    if (v->deathOnCaptureTypes & v->petrifyOnCaptureTypes)
    {
        if (DoCheck)
            std::cerr << "deathOnCaptureTypes and petrifyOnCaptureTypes cannot overlap." << std::endl;
        return false;
    }
    parse_attribute("petrifyOnCaptureSuppressTransfer", v->petrifyOnCaptureSuppressTransfer);
    parse_attribute("petrifyBlastPieces", v->petrifyBlastPieces);
    parse_attribute("removeConnectN", v->removeConnectN);
    if (v->removeConnectN < 0 || v->removeConnectN > int(SQUARE_NB)) {
        if (DoCheck)
            std::cerr << "removeConnectN - Value must be in range [0, " << int(SQUARE_NB) << "]. Clamping." << std::endl;
        v->removeConnectN = std::clamp(v->removeConnectN, 0, int(SQUARE_NB));
    }
    parse_attribute("removeConnectNByType", v->removeConnectNByType);
    parse_attribute("surroundCaptureOpposite", v->surroundCaptureOpposite);
    parse_attribute("surroundCaptureIntervene", v->surroundCaptureIntervene);
    parse_attribute("surroundCaptureEdge", v->surroundCaptureEdge);
    parse_attribute("surroundCaptureMaxRegion", v->surroundCaptureMaxRegion);
    parse_attribute("surroundCaptureHostileRegion", v->surroundCaptureHostileRegion);
    parse_attribute("doubleStep", v->doubleStep);
    parse_color_setting("doubleStepRegion", v->doubleStepRegion);
    parse_color_setting("tripleStepRegion", v->tripleStepRegion);
    parse_color_setting("enPassantRegion", v->enPassantRegion);
    if (!parse_color_setting_piece("enPassantTypes", v->enPassantTypes, v)) return false;
    parse_attribute("enPassantPassedSquares", v->enPassantPassedSquares);
    parse_attribute("castling", v->castling);
    parse_attribute("castlingDroppedPiece", v->castlingDroppedPiece);
    parse_attribute("castlingPromotedPiece", v->castlingPromotedPiece);
    parse_attribute("castlingForbiddenPlies", v->castlingForbiddenPlies);
    parse_attribute("castlingKingsideFile", v->castlingKingsideFile);
    parse_attribute("castlingQueensideFile", v->castlingQueensideFile);
    parse_attribute("castlingRank", v->castlingRank);
    parse_attribute("castlingKingFile", v->castlingKingFile);
    if (!parse_color_setting_piece("castlingKingPiece", v->castlingKingPiece, v)) return false;
    parse_attribute("castlingRookKingsideFile", v->castlingRookKingsideFile);
    parse_attribute("castlingRookQueensideFile", v->castlingRookQueensideFile);
    if (!parse_color_setting_piece("castlingRookPieces", v->castlingRookPieces, v)) return false;
    parse_attribute("oppositeCastling", v->oppositeCastling);
    parse_attribute("checking", v->checking);
    parse_attribute("allowChecks", v->allowChecks);
    parse_attribute("royalPieceNoThroughCheck", v->royalPieceNoThroughCheck);
    parse_color_setting("dropChecks", v->dropChecks);
    parse_color_setting("dropMates", v->dropMates);
    parse_color_setting("mustCapture", v->mustCapture);
    parse_color_setting("mustCaptureEnPassant", v->mustCaptureEnPassant);
    parse_attribute("rifleCapture", v->rifleCapture);
    const auto& it_push_strength = config.find("pushingStrength");
    if (it_push_strength != config.end())
    {
        if (!parse_piece_int_map_option("pushingStrength", it_push_strength->second, v, v->pushingStrength, true, DoCheck, [&](int (&parsedStrengths)[PIECE_TYPE_NB]) {
            for (PieceType pt = PAWN; pt < PIECE_TYPE_NB; ++pt)
            {
                if (parsedStrengths[pt] < 0)
                {
                    if (DoCheck)
                        std::cerr << "pushingStrength - Invalid negative value." << std::endl;
                    return false;
                }
            }
            return true;
        }))
            return false;
    }
    const auto& it_pull_strength = config.find("pullingStrength");
    if (it_pull_strength != config.end())
    {
        if (!parse_piece_int_map_option("pullingStrength", it_pull_strength->second, v, v->pullingStrength, true, DoCheck, [&](int (&parsedStrengths)[PIECE_TYPE_NB]) {
            for (PieceType pt = PAWN; pt < PIECE_TYPE_NB; ++pt)
            {
                if (parsedStrengths[pt] < 0)
                {
                    if (DoCheck)
                        std::cerr << "pullingStrength - Invalid negative value." << std::endl;
                    return false;
                }
            }
            return true;
        }))
            return false;
    }
    parse_attribute("pushFirstColor", v->pushFirstColor);
    parse_attribute("pushingRemoves", v->pushingRemoves);
    parse_attribute("pushChainEnemyOnly", v->pushChainEnemyOnly);
    parse_attribute("pushCaptureAgainstFriendlyBlocker", v->pushCaptureAgainstFriendlyBlocker);
    parse_attribute("pushNoImmediateReturn", v->pushNoImmediateReturn);
    parse_attribute("stepwisePushing", v->stepwisePushing);
    parse_attribute("adjacentSwapMoveTypes", v->adjacentSwapMoveTypes, v);
    parse_attribute("adjacentSwapRequiresEmptyNeighbor", v->adjacentSwapRequiresEmptyNeighbor);
    parse_attribute("swapNoImmediateReturn", v->swapNoImmediateReturn);
    parse_attribute("swapForbiddenPlies", v->swapForbiddenPlies);
    parse_attribute("edgeInsertTypes", v->edgeInsertTypes, v);
    parse_attribute("edgeInsertOnly", v->edgeInsertOnly);
    parse_color_setting("edgeInsertRegion", v->edgeInsertRegion);
    const auto& it_edge_insert_from = config.find("edgeInsertFrom");
    if (it_edge_insert_from != config.end())
    {
        bool top = false, bottom = false, left = false, right = false;
        if (!apply_edge_insert_from_alias(it_edge_insert_from->second, top, bottom, left, right))
        {
            if (DoCheck)
                std::cerr << "edgeInsertFrom - Invalid syntax." << std::endl;
            return false;
        }
        v->edgeInsertFromTop.set_global(top);
        v->edgeInsertFromBottom.set_global(bottom);
        v->edgeInsertFromLeft.set_global(left);
        v->edgeInsertFromRight.set_global(right);
    }
    const auto& it_edge_insert_from_white = config.find("edgeInsertFromWhite");
    if (it_edge_insert_from_white != config.end())
    {
        bool top = false, bottom = false, left = false, right = false;
        if (!apply_edge_insert_from_alias(it_edge_insert_from_white->second, top, bottom, left, right))
        {
            if (DoCheck)
                std::cerr << "edgeInsertFromWhite - Invalid syntax." << std::endl;
            return false;
        }
        v->edgeInsertFromTop.set_color(WHITE, top);
        v->edgeInsertFromBottom.set_color(WHITE, bottom);
        v->edgeInsertFromLeft.set_color(WHITE, left);
        v->edgeInsertFromRight.set_color(WHITE, right);
    }
    const auto& it_edge_insert_from_black = config.find("edgeInsertFromBlack");
    if (it_edge_insert_from_black != config.end())
    {
        bool top = false, bottom = false, left = false, right = false;
        if (!apply_edge_insert_from_alias(it_edge_insert_from_black->second, top, bottom, left, right))
        {
            if (DoCheck)
                std::cerr << "edgeInsertFromBlack - Invalid syntax." << std::endl;
            return false;
        }
        v->edgeInsertFromTop.set_color(BLACK, top);
        v->edgeInsertFromBottom.set_color(BLACK, bottom);
        v->edgeInsertFromLeft.set_color(BLACK, left);
        v->edgeInsertFromRight.set_color(BLACK, right);
    }
    parse_attribute("changingColorTrigger", v->changingColorTrigger);
    parse_attribute("changingColorPieceTypes", v->changingColorPieceTypes, v);
    parse_color_setting("selfCapture", v->selfCapture);
    if (!parse_color_setting_piece("selfCaptureTypes", v->selfCaptureTypes, v)) return false;
    parse_attribute("blastOnSameTypeCapture", v->blastOnSameTypeCapture);
    parse_attribute("captureMorph", v->captureMorph);
    parse_attribute("rexExclusiveMorph", v->rexExclusiveMorph);
    parse_color_setting("mustDrop", v->mustDrop);
    if (!parse_color_setting_piece("mustDropType", v->mustDropType, v)) return false;
    parse_attribute("openingSwapDrop", v->openingSwapDrop);
    parse_attribute("openingSwapMirrorMainDiagonal", v->openingSwapMirrorMainDiagonal);
    parse_attribute("dropKingLast", v->dropKingLast);
    parse_attribute("openingSelfRemoval", v->openingSelfRemoval);
    parse_attribute("openingSelfRemovalAdjacentToLast", v->openingSelfRemovalAdjacentToLast);
    parse_color_setting("openingSelfRemovalRegion", v->openingSelfRemovalRegion);
    parse_attribute("pieceDrops", v->pieceDrops);
    parse_attribute("borrowOpponentDropsWhenEmpty", v->borrowOpponentDropsWhenEmpty);
    parse_attribute("virtualDrops", v->virtualDrops);
    const auto& it_virtual_drop_limit = config.find("virtualDropLimit");
    if (it_virtual_drop_limit != config.end())
    {
        if (!parse_piece_int_map_option("virtualDropLimit", it_virtual_drop_limit->second, v, v->virtualDropLimit, true, DoCheck, [&](int (&parsedLimits)[PIECE_TYPE_NB]) {
            for (PieceType pt = PAWN; pt < PIECE_TYPE_NB; ++pt)
                if (parsedLimits[pt] < 0)
                {
                    if (DoCheck)
                        std::cerr << "virtualDropLimit - Invalid negative value." << std::endl;
                    return false;
                }
            return true;
        }))
            return false;
        v->virtualDropLimitEnabled = true;
    }
    parse_attribute("dropLoop", v->dropLoop);

    bool capturesToHand = false;
    if (parse_attribute<false>("capturesToHand", capturesToHand)) {
        v->captureType = capturesToHand ? HAND : MOVE_OUT;
    }

    parse_attribute("captureType", v->captureType);
    parse_attribute("captureToHandSide", v->captureToHandSide);
    parse_attribute("captureToHandTypes", v->captureToHandTypes, v);
    // hostage price
    const auto& it_host_p = config.find("hostageExchange");
    if (it_host_p != config.end()) {
        if (!parse_hostage_exchanges(v, it_host_p->second, DoCheck))
            return false;
    }
    parse_attribute("prisonPawnPromotion", v->prisonPawnPromotion);
    parse_attribute("firstRankPawnDrops", v->firstRankPawnDrops);
    parse_attribute("promotionZonePawnDrops", v->promotionZonePawnDrops);
    parse_attribute("enclosingDrop", v->enclosingDrop);
    parse_attribute("enclosingDropStart", v->enclosingDropStart);
    parse_color_setting("dropRegion", v->dropRegion);
    parse_attribute("sittuyinRookDrop", v->sittuyinRookDrop);
    parse_attribute("dropOppositeColoredBishop", v->dropOppositeColoredBishop);
    parse_attribute("dropPromoted", v->dropPromoted);
    parse_attribute("symmetricDropTypes", v->symmetricDropTypes, v);
    parse_attribute("captureDrops", v->captureDrops, v);
    if (!parse_color_setting_piece("dropNoDoubled", v->dropNoDoubled, v)) return false;
    parse_color_setting("dropNoDoubledCount", v->dropNoDoubledCount);
    parse_attribute("freeDrops", v->freeDrops);
    parse_attribute("payPointsToDrop", v->payPointsToDrop);
    parse_attribute("potions", v->potions);
    parse_attribute("freezePotion", v->potionPiece[Variant::POTION_FREEZE], v);
    parse_attribute("jumpPotion", v->potionPiece[Variant::POTION_JUMP], v);
    parse_attribute("freezeCooldown", v->potionCooldown[Variant::POTION_FREEZE]);
    parse_attribute("jumpCooldown", v->potionCooldown[Variant::POTION_JUMP]);
    if (v->potionPiece[Variant::POTION_FREEZE] != NO_PIECE_TYPE
        && v->potionPiece[Variant::POTION_FREEZE] == v->potionPiece[Variant::POTION_JUMP])
    {
        if (DoCheck)
            std::cerr << "freezePotion and jumpPotion must use different piece types." << std::endl;
        return false;
    }
    parse_attribute("potionDropOnOccupied", v->potionDropOnOccupied);
    parse_attribute("immobilityIllegal", v->immobilityIllegal);
    parse_attribute("gating", v->gating);
    parse_attribute("wallingRule", v->wallingRule);
    parse_color_setting("walling", v->wallingSide);
    parse_color_setting("wallingRegion", v->wallingRegion);
    parse_attribute("wallOrMove", v->wallOrMove);
    parse_attribute("surroundClaimRegion", v->surroundClaimRegion);
    parse_attribute("surroundClaimPiece", v->surroundClaimPiece, v);
    parse_attribute("surroundClaimExtraTurn", v->surroundClaimExtraTurn);
    parse_attribute("gatingFromHand", v->gatingFromHand);
    parse_attribute("seirawanGating", v->seirawanGating);
    parse_attribute("commitGates", v->commitGates);
    parse_attribute("cloneMoveTypes", v->cloneMoveTypes, v);
    if (v->cloneMoveTypes & PAWN)
    {
        if (DoCheck)
            std::cerr << "cloneMoveTypes - PAWN is not supported for clone moves and will be ignored." << std::endl;
        v->cloneMoveTypes &= ~piece_set(PAWN);
    }
    parse_attribute("forcedJumpContinuation", v->forcedJumpContinuation);
    parse_attribute("forcedJumpSameDirection", v->forcedJumpSameDirection);
    parse_attribute("cambodianMoves", v->cambodianMoves);
    parse_attribute("firstMoveLoseOnCheck", v->firstMoveLoseOnCheck);
    parse_attribute("diagonalLines", v->diagonalLines);
    parse_color_setting("pass", v->pass);
    parse_color_setting("passOnStalemate", v->passOnStalemate);
    parse_attribute("passUntilSetup", v->passUntilSetup);
    if (config.count("multimoves") && !parse_attribute("multimoves", v->multimoves))
        return false;
    if (config.count("multimoves"))
    {
        for (int n : v->multimoves)
            if (n <= 0)
            {
                if (DoCheck)
                    std::cerr << "multimoves - Invalid non-positive value." << std::endl;
                return false;
            }
    }
    parse_attribute("progressiveMultimove", v->progressiveMultimove);
    if (DoCheck)
    {
        int usedPly = 0;
        size_t usedEntries = 0;
        for (int n : v->multimoves)
        {
            int segment = 2 * n - 1;
            if (segment <= 0 || usedPly + segment >= START_MULTIMOVES)
                break;
            usedPly += segment;
            ++usedEntries;
        }
        if (usedEntries < v->multimoves.size())
            std::cerr << "multimoves - start pattern exceeds START_MULTIMOVES (" << START_MULTIMOVES
                      << "), tail entries will be ignored." << std::endl;
    }
    parse_attribute("multimoveCheck", v->multimoveCheck);
    parse_attribute("multimoveCapture", v->multimoveCapture);
    parse_attribute("makpongRule", v->makpongRule);
    parse_attribute("flyingGeneral", v->flyingGeneral);
    parse_attribute("diagonalGeneral", v->diagonalGeneral);
    parse_attribute("soldierPromotionRank", v->soldierPromotionRank);
    parse_attribute("flipEnclosedPieces", v->flipEnclosedPieces);
    // game end
    if (!parse_color_setting_piece("nMoveRuleTypes", v->nMoveRuleTypes, v)) return false;
    parse_attribute("nMoveRule", v->nMoveRule);
    parse_attribute("nMoveRuleImmediate", v->nMoveRuleImmediate);
    parse_attribute("nMoveHardLimitRule", v->nMoveHardLimitRule);
    parse_attribute("nMoveHardLimitRuleValue", v->nMoveHardLimitRuleValue);
    parse_attribute("nFoldRule", v->nFoldRule);
    parse_attribute("nFoldRuleImmediate", v->nFoldRuleImmediate);
    parse_color_setting("nFoldValue", v->nFoldValue);
    parse_attribute("nFoldValueAbsolute", v->nFoldValueAbsolute);
    if (v->nFoldValueAbsolute) {
        if (!v->nFoldValue.byColorSet[WHITE]) {
            v->nFoldValue.byColor[WHITE] = v->nFoldValue.global;
            v->nFoldValue.byColorSet[WHITE] = true;
        }
        if (!v->nFoldValue.byColorSet[BLACK]) {
            v->nFoldValue.byColor[BLACK] = -v->nFoldValue.global;
            v->nFoldValue.byColorSet[BLACK] = true;
        }
    }
    parse_attribute("perpetualCheckIllegal", v->perpetualCheckIllegal);
    parse_attribute("moveRepetitionIllegal", v->moveRepetitionIllegal);
    parse_attribute("samePlayerBoardRepetitionIllegal", v->samePlayerBoardRepetitionIllegal);
    parse_attribute("alternating2x2DropIllegal", v->alternating2x2DropIllegal);
    parse_attribute("pathwayDropRule", v->pathwayDropRule);
    parse_attribute("weakDiagonalConnect", v->weakDiagonalConnect);
    parse_attribute("reciprocalWeakConnectionDrop", v->reciprocalWeakConnectionDrop);
    parse_attribute("weakCrosscutDropIllegal", v->weakCrosscutDropIllegal);
    parse_attribute("weakConnectionNobiImpossible", v->weakConnectionNobiImpossible);
    parse_attribute("chasingRule", v->chasingRule);
    parse_color_setting("stalemateValue", v->stalemateValue);
    parse_attribute("stalematePieceCount", v->stalematePieceCount);
    parse_color_setting("checkmateValue", v->checkmateValue);
    parse_color_setting("shogiPawnDropMateIllegal", v->shogiPawnDropMateIllegal);
    parse_attribute("shatarMateRule", v->shatarMateRule);
    parse_attribute("bikjangRule", v->bikjangRule);
    parse_attribute("pseudoRoyalTypes", v->pseudoRoyalTypes, v);
    parse_attribute("pseudoRoyalCount", v->pseudoRoyalCount);
    parse_attribute("pseudoRoyalValue", v->pseudoRoyalValue);
    parse_attribute("pseudoRoyalCaptureIllegal", v->pseudoRoyalCaptureIllegal);
    parse_attribute("antiRoyalTypes", v->antiRoyalTypes, v);
    parse_attribute("antiRoyalCount", v->antiRoyalCount);
    parse_attribute("antiRoyalSelfCaptureOnly", v->antiRoyalSelfCaptureOnly);
    parse_attribute("antiRoyalKingMutuallyImmune", v->antiRoyalKingMutuallyImmune);
    parse_color_setting("extinctionValue", v->extinctionValue);
    parse_attribute("extinctionClaim", v->extinctionClaim);
    parse_attribute("extinctionPseudoRoyal", v->extinctionPseudoRoyal);
    parse_attribute("dupleCheck", v->dupleCheck);
    // extinction piece types
    parse_attribute("extinctionMustAppear", v->extinctionMustAppear, v);
    if (!parse_color_setting_piece("extinctionPieceTypes", v->extinctionPieceTypes, v)) return false;
    parse_color_setting("extinctionAllPieceTypes", v->extinctionAllPieceTypes);
    parse_color_setting("extinctionPieceCount", v->extinctionPieceCount);
    parse_color_setting("extinctionOpponentPieceCount", v->extinctionOpponentPieceCount);

    // Backward compatibility for legacy extinctionPseudoRoyal configs.
    if (v->extinctionPseudoRoyal && !v->pseudoRoyalTypes)
    {
        v->pseudoRoyalTypes = v->extinctionPieceTypes;
        v->pseudoRoyalCount = v->extinctionPieceCount + 1;
    }
    if (!parse_color_setting_piece("flagPiece", v->flagPiece, v)) return false;
    parse_color_setting("flagRegion", v->flagRegion);
    parse_attribute("flagPieceCount", v->flagPieceCount);
    parse_attribute("flagPieceBlockedWin", v->flagPieceBlockedWin);
    parse_attribute("flagMove", v->flagMove);
    parse_attribute("flagPieceSafe", v->flagPieceSafe);
    parse_attribute("checkCounting", v->checkCounting);
    parse_attribute("connectN", v->connectN);
    parse_attribute("connectPieceTypes", v->connectPieceTypes, v);
    parse_attribute("connectGoalByType", v->connectGoalByType);
    parse_color_setting("connectPieceGoal", v->connectPieceGoal);
    parse_attribute("connectHorizontal", v->connectHorizontal);
    parse_attribute("connectVertical", v->connectVertical);
    parse_attribute("connectDiagonal", v->connectDiagonal);
    parse_attribute("connectNorthEast", v->connectNorthEast);
    parse_attribute("connectSouthEast", v->connectSouthEast);
    parse_attribute("connect3D", v->connect3D);
    parse_attribute("connect4D", v->connect4D);
    parse_color_setting("connectRegion1", v->connectRegion1);
    parse_color_setting("connectRegion2", v->connectRegion2);
    parse_color_setting("connectRegion3", v->connectRegion3);
    parse_attribute("connectNxN", v->connectNxN);
    parse_attribute("collinearN", v->collinearN);
    parse_attribute("connectGroup", v->connectGroup);
    parse_attribute("connectValue", v->connectValue);
    parse_attribute("materialCounting", v->materialCounting);
    parse_attribute("materialCountingPieceTypes", v->materialCountingPieceTypes, v);
    parse_attribute("adjudicateFullBoard", v->adjudicateFullBoard);
    parse_attribute("countingRule", v->countingRule);
    parse_attribute("castlingWins", v->castlingWins);
    parse_attribute("pointsCounting", v->pointsCounting);
    parse_attribute("pointsRuleCaptures", v->pointsRuleCaptures);
    parse_attribute("pointsGoal", v->pointsGoal);
    parse_attribute("pointsGoalValue", v->pointsGoalValue);
    parse_attribute("pointsGoalSimulValueByMover", v->pointsGoalSimulValueByMover);
    parse_attribute("pointsGoalSimulValueByMostPoints", v->pointsGoalSimulValueByMostPoints);
    if (config.find("pointsGoalSimulValueByMostPoints") == config.end())
        parse_attribute("pointsGoalSimulValue", v->pointsGoalSimulValueByMostPoints);
    if (v->payPointsToDrop)
        v->pointsCounting = true;

    // Report invalid options
    if (DoCheck)
    {
        const std::set<std::string>& parsedKeys = config.get_consumed_keys();
        for (const auto& it : config)
            if (parsedKeys.find(it.first) == parsedKeys.end())
            {
                std::cerr << "Invalid option: " << it.first << std::endl;
                if (looks_like_piece_definition_value(it.second))
                    std::cerr << it.first << " looks like a custom piece definition. Use customPieceN = "
                              << it.second << " for new custom pieces." << std::endl;
            }
    }
    return true;
}

template <bool DoCheck>
bool VariantParser<DoCheck>::check_consistency(Variant* v) {
    bool valid = true;

    const bool wrapsTopology = v->cylindrical || v->toroidal;
    v->rebuild_piece_symbol_maps();
    const bool hasRoyalKing = v->checking
                           && v->kingType != NO_PIECE_TYPE
                           && bool(v->pieceTypes & piece_set(v->kingType));

    // pieces
    if (DoCheck)
        for (PieceSet ps = v->pieceTypes; ps;)
        {
            PieceType pt = pop_lsb(ps);
            for (Color c : {WHITE, BLACK})
            {
                Piece pc = make_piece(c, pt);
                const std::string& symbol = v->piece_symbol(pc);
                if (symbol.empty())
                    continue;
                auto exact = v->symbolToPiece.find(symbol);
                if (exact == v->symbolToPiece.end() || exact->second != pc)
                    std::cerr << piece_name(pt) << " - Ambiguous piece symbol: " << symbol << std::endl;
            }
        }

    v->conclude(); // In preparation for the consistency checks below

    // startFen
    if (FEN::validate_fen(v->startFen, v, v->chess960) != FEN::FEN_OK)
    {
        if (DoCheck)
            std::cerr << "startFen - Invalid starting position: " << v->startFen << std::endl;
        valid = false;
    }

    if (v->hexBoard && wrapsTopology)
    {
        if (DoCheck)
            std::cerr << "hexBoard is not supported together with cylindrical or toroidal topology." << std::endl;
        valid = false;
    }

    // pieceToCharTable
    if (DoCheck && v->pieceToCharTable != "-")
    {
        const std::string fenBoard = v->startFen.substr(0, v->startFen.find(' '));
        std::stringstream ss(v->pieceToCharTable);
        char token;
        while (ss >> token)
            if (std::isalpha(static_cast<unsigned char>(token))
                && v->pieceToChar.find(std::toupper(static_cast<unsigned char>(token))) == std::string::npos)
                std::cerr << "pieceToCharTable - Invalid piece type: " << token << std::endl;
        for (PieceSet ps = v->pieceTypes; ps;)
        {
            PieceType pt = pop_lsb(ps);
            char ptl = std::tolower(static_cast<unsigned char>(v->pieceToChar[pt]));
            if (v->pieceToCharTable.find(ptl) == std::string::npos && fenBoard.find(ptl) != std::string::npos)
                std::cerr << "pieceToCharTable - Missing piece type: " << ptl << std::endl;
            char ptu = std::toupper(static_cast<unsigned char>(v->pieceToChar[pt]));
            if (v->pieceToCharTable.find(ptu) == std::string::npos && fenBoard.find(ptu) != std::string::npos)
                std::cerr << "pieceToCharTable - Missing piece type: " << ptu << std::endl;
        }
    }

    // Contradictory options
    if (DoCheck && !v->checking && v->checkCounting)
        std::cerr << "checkCounting=true requires checking=true." << std::endl;
    if (DoCheck && !v->checking && v->allowChecks)
        std::cerr << "checking=false with allowChecks=true is unusual: king safety is disabled, so the no-check rule will not constrain legality." << std::endl;
    if (DoCheck && v->progressiveMultimove && !v->multimoves.empty())
        std::cerr << "progressiveMultimove ignores multimoves sequence." << std::endl;
    for (Color c : {WHITE, BLACK})
    {
        std::stringstream ss(v->connectPieceGoal[c]);
        std::string token;
        while (ss >> token)
            if (parse_piece_type_token(v, token) == NO_PIECE_TYPE)
            {
                if (DoCheck)
                    std::cerr << "connectPieceGoal" << (c == WHITE ? "White" : "Black")
                              << " - Invalid piece type: " << token << std::endl;
                valid = false;
                break;
            }
    }
    if (v->castling && v->castlingRank > v->maxRank)
    {
        if (DoCheck)
            std::cerr << "Inconsistent settings: castlingRank > maxRank." << std::endl;
        valid = false;
    }
    if (v->castling && v->castlingQueensideFile > v->castlingKingsideFile)
    {
        if (DoCheck)
            std::cerr << "Inconsistent settings: castlingQueensideFile > castlingKingsideFile." << std::endl;
        valid = false;
    }
    if (v->castling)
    {
        int kingFile = int(v->castlingKingFile);
        int kingSide = int(v->castlingKingsideFile);
        int queenSide = int(v->castlingQueensideFile);
        if (std::abs(kingSide - kingFile) <= 1 || std::abs(queenSide - kingFile) <= 1)
        {
            if (DoCheck)
            std::cerr << "Castling destination is adjacent to castlingKingFile; some GUIs/protocols may not distinguish castling from a normal king move." << std::endl;
        }
    }
    if (v->connect3D && v->connect4D)
    {
        if (DoCheck)
            std::cerr << "connect3D and connect4D are mutually exclusive." << std::endl;
        valid = false;
    }
    if (v->connect3D
        && !((v->connectN == 3 && (int(v->maxFile) + 1) == 3 && (int(v->maxRank) + 1) == 9)
          || (v->connectN == 4 && (int(v->maxFile) + 1) == 8 && (int(v->maxRank) + 1) == 8)))
    {
        if (DoCheck)
            std::cerr << "connect3D currently requires either connectN = 3 on a 3x9 board or connectN = 4 on an 8x8 board." << std::endl;
        valid = false;
    }
    if (v->connect4D
        && !(v->connectN >= 3
          && (int(v->maxFile) + 1) == v->connectN * v->connectN
          && (int(v->maxRank) + 1) == v->connectN * v->connectN))
    {
        if (DoCheck)
            std::cerr << "connect4D currently requires a square board of size connectN^2 with connectN >= 3." << std::endl;
        valid = false;
    }
    if (wrapsTopology && (v->connectN || v->connect3D || v->connect4D || v->connectNxN || v->collinearN || v->connectGroup || v->removeConnectN))
    {
        if (DoCheck)
            std::cerr << "Wrapped boards do not support connect/collinear win conditions." << std::endl;
        valid = false;
    }

    if (wrapsTopology)
        for (int i = 0; i < CUSTOM_PIECES_NB; ++i)
            if (!v->customPiece[i].empty() && (v->customPiece[i].find('x') != std::string::npos || v->customPiece[i].find('z') != std::string::npos))
            {
                if (DoCheck)
                    std::cerr << "Wrapped boards do not support x/z rider modifiers in customPiece"
                              << (i + 1) << "." << std::endl;
                valid = false;
                break;
            }

    // Check for limitations
    if ((v->pieceDrops || v->freeDrops) && v->wallingRule != NO_WALLING)
    {
        if (DoCheck)
            std::cerr << "pieceDrops and any walling are incompatible." << std::endl;
        valid = false;
    }
    if (v->edgeInsertTypes && !v->pieceDrops)
    {
        if (DoCheck)
            std::cerr << "edgeInsertTypes requires pieceDrops=true." << std::endl;
        valid = false;
    }
    if (v->openingSwapDrop
        && (!v->pieceDrops
            || !v->mustDrop
            || v->captureType != MOVE_OUT
            || v->selfCapture
            || v->selfCaptureTypes != NO_PIECE_SET
            || v->selfCapture.get(WHITE)
            || v->selfCapture.get(BLACK)
            || v->selfCaptureTypes.get(WHITE) != NO_PIECE_SET
            || v->selfCaptureTypes.get(BLACK) != NO_PIECE_SET
            || v->captureDrops
            || v->symmetricDropTypes
            || v->twoBoards
            || v->freeDrops
            || v->edgeInsertTypes))
    {
        if (DoCheck)
            std::cerr << "openingSwapDrop is only supported for simple move-out mandatory drop variants without capture drops, paired drops, self capture, free drops, edge inserts, or two-board reserves." << std::endl;
        valid = false;
    }
    if (v->openingSwapMirrorMainDiagonal
        && (!v->openingSwapDrop
            || int(v->maxFile) != int(v->maxRank)))
    {
        if (DoCheck)
            std::cerr << "openingSwapMirrorMainDiagonal requires openingSwapDrop on a square board." << std::endl;
        valid = false;
    }

    bool hasGatingPieceAfter = false;
    for (Color c : {WHITE, BLACK})
        for (int i = 0; i < PIECE_TYPE_NB; ++i)
            if (v->gatingPieceAfter[c][i] != NO_PIECE_TYPE)
                hasGatingPieceAfter = true;

    if (v->wallingRule != NO_WALLING && (v->seirawanGating || v->potions || v->gating || hasGatingPieceAfter))
    {
        if (DoCheck)
            std::cerr << "wallingRule and gating features (seirawanGating, potions, gating, gatingPieceAfter) are incompatible." << std::endl;
        valid = false;
    }
    if (v->wallingRule == DUCK && v->petrifyOnCaptureTypes)
    {
        if (DoCheck)
            std::cerr << "wallingRule=duck and petrifyOnCaptureTypes are incompatible." << std::endl;
        valid = false;
    }

    if (hasGatingPieceAfter && (v->seirawanGating || v->potions))
    {
        if (DoCheck)
            std::cerr << "gatingPieceAfter and other gating features (seirawanGating, potions) are incompatible." << std::endl;
        valid = false;
    }
    if (v->seirawanGating && v->potions)
    {
        if (DoCheck)
            std::cerr << "seirawanGating and potions are incompatible." << std::endl;
        valid = false;
    }
    if (v->potions && v->gating)
    {
        if (DoCheck)
            std::cerr << "potions and gating are incompatible." << std::endl;
        valid = false;
    }

    // Options incompatible with royal kings
    if (hasRoyalKing)
    {
        if (v->blastOnCapture)
        {
            if (DoCheck)
                std::cerr << "Can not use kings with blastOnCapture." << std::endl;
            valid = false;
        }
        if (v->blastPassiveTypes)
        {
            if (DoCheck)
                std::cerr << "Can not use kings with blastPassiveTypes." << std::endl;
            valid = false;
        }
        if (v->flipEnclosedPieces)
        {
            if (DoCheck)
                std::cerr << "Can not use kings with flipEnclosedPieces." << std::endl;
            valid = false;
        }
        if (v->wallingRule==DUCK)
        {
            if (DoCheck)
                std::cerr << "Can not use kings with wallingRule = duck." << std::endl;
            valid = false;
        }
        // We can not fully check support for custom king movements at this point,
        // since custom pieces are only initialized on loading of the variant.
        // We will assume this is valid, but it might cause problems later if it's not.
        if (!is_custom(v->kingType))
        {
            const PieceInfo* pi = pieceMap.find(v->kingType)->second;
            if (   pi->hopper[0][MODALITY_QUIET].size()
                || pi->hopper[0][MODALITY_CAPTURE].size()
                || std::any_of(pi->steps[0][MODALITY_CAPTURE].begin(),
                               pi->steps[0][MODALITY_CAPTURE].end(),
                               [](const std::pair<const Direction, int>& d) { return d.second; }))
            {
                if (DoCheck)
                    std::cerr << piece_name(v->kingType) << " is not supported as kingType." << std::endl;
                valid = false;
            }
        }
    }

    if (v->removeConnectN)
    {
        if (hasRoyalKing || v->pseudoRoyalTypes || v->antiRoyalTypes)
        {
            if (DoCheck)
                std::cerr << "removeConnectN is incompatible with (pseudo/anti-)royal pieces." << std::endl;
            valid = false;
        }
        if (v->connectN || v->connect3D || v->connect4D || v->connectNxN || v->collinearN || v->connectGroup
            || v->connectRegion1[WHITE] || v->connectRegion1[BLACK]
            || !v->connectPieceGoal[WHITE].empty() || !v->connectPieceGoal[BLACK].empty())
        {
            if (DoCheck)
                std::cerr << "removeConnectN is incompatible with connection win conditions." << std::endl;
            valid = false;
        }
    }

    if (v->hexBoard && (v->reciprocalWeakConnectionDrop || v->weakCrosscutDropIllegal || v->weakConnectionNobiImpossible))
    {
        if (DoCheck)
            std::cerr << "Hex boards do not support square weak-connection drop rules." << std::endl;
        valid = false;
    }

    // Options incompatible with royal kings OR pseudo-royal kings. Possible in theory though:
    // 1. In blast variants, moving a (pseudo-)royal blastImmuneType into another piece is legal.
    // 2. In blast variants, capturing a piece next to a (pseudo-)royal blastImmuneType is legal.
    // 3. Moving a (pseudo-)royal mutuallyImmuneType into a square threatened by the same type is legal.
    if (v->pseudoRoyalTypes || v->antiRoyalTypes || hasRoyalKing)
    {
        if (v->blastImmuneTypes) //I may have this solved now.
        {
            if (DoCheck)
                std::cerr << "Can not use kings, pseudo-royal, or anti-royal with blastImmuneTypes." << std::endl;
            valid = false;
        }
        if (v->mutuallyImmuneTypes)
        {
            if (DoCheck)
                std::cerr << "Can not use kings, pseudo-royal, or anti-royal with mutuallyImmuneTypes." << std::endl;
            valid = false;
        }
        if (v->antiRoyalTypes & v->pseudoRoyalTypes)
        {
            if (DoCheck)
                std::cerr << "Piece can not be both pseudo-royal and anti-royal." << std::endl;
            valid = false;
        }
        if (v->antiRoyalTypes & KING)
        {
            if (DoCheck)
                std::cerr << "Piece can not be both royal king and anti-royal." << std::endl;
            valid = false;
        }
    }
    if (v->flagPieceSafe)
    {
        if (v->blastOnCapture)
        {
            if (DoCheck)
                std::cerr << "Can not use flagPieceSafe with blastOnCapture (flagPieceSafe uses simple assessment that does not see blast)." << std::endl;
            valid = false;
        }
        if ((v->antiRoyalTypes & v->flagPiece.get(WHITE)) || (v->antiRoyalTypes & v->flagPiece.get(BLACK)))
        {
            if (DoCheck)
                std::cerr << "Flag piece can not be anti-royal when flagPieceSafe is enabled." << std::endl;
            valid = false;
        }
    }
    return valid;
}

template <bool DoCheck>
Variant* VariantParser<DoCheck>::parse() {
    auto v = std::make_unique<Variant>();
    v->reset_pieces();
    Variant* parsed = parse(v.get());
    return parsed ? v.release() : nullptr;
}

template <bool DoCheck>
Variant* VariantParser<DoCheck>::parse(Variant* v) {
    parseHadError = false;
    int cfgMaxRank = -1;
    int cfgMaxFile = -1;
    const auto itRank = config.find("maxRank");
    if (itRank != config.end())
        parse_rank_index(itRank->second, cfgMaxRank);
    const auto itFile = config.find("maxFile");
    if (itFile != config.end())
        parse_file_index(itFile->second, cfgMaxFile);

    // Fail early when a variant exceeds compile-time board dimensions.
    if ((cfgMaxRank >= 0 && cfgMaxRank > RANK_MAX) || (cfgMaxFile >= 0 && cfgMaxFile > FILE_MAX))
        return nullptr;

    if (itRank != config.end())
    {
        int parsedRank = 0;
        if (!parse_rank_index(itRank->second, parsedRank))
            return nullptr;
        v->maxRank = Rank(parsedRank);
    }
    if (itFile != config.end())
    {
        int parsedFile = 0;
        if (!parse_file_index(itFile->second, parsedFile))
            return nullptr;
        v->maxFile = File(parsedFile);
    }

    if (!parse_piece_types(v) ||
        !parse_piece_values(v) ||
        !parse_legacy_attributes(v) ||
        !parse_official_options(v))
        return nullptr;

    if (parseHadError)
        return nullptr;

    if (!check_consistency(v))
        return nullptr;

    return v;
}

template Variant* VariantParser<true>::parse();
template Variant* VariantParser<false>::parse();
template Variant* VariantParser<true>::parse(Variant* v);
template Variant* VariantParser<false>::parse(Variant* v);

} // namespace Stockfish
