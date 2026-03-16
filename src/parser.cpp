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
#include <cctype>
#include <charconv>
#include <memory>

#include "apiutil.h"
#include "parser.h"
#include "piece.h"
#include "types.h"

namespace Stockfish {

namespace {
    bool only_trailing_space(std::stringstream& ss) {
        ss >> std::ws;
        return ss.eof();
    }

    bool parse_positive_int(const std::string& value, int& out) {
        if (value.empty())
            return false;

        const char* first = value.data();
        const char* last  = first + value.size();
        auto [ptr, ec] = std::from_chars(first, last, out);
        return ec == std::errc() && ptr == last && out >= 1;
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
        auto [ptr, ec] = std::from_chars(first, last, target);
        while (ptr != last && std::isspace(static_cast<unsigned char>(*ptr)))
            ptr++;
        return ec == std::errc() && ptr == last;
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
        std::stringstream ss(value);
        int i;
        ss >> i;
        Rank parsed = Rank(i - 1);
        if (ss.fail() || !only_trailing_space(ss) || parsed < RANK_1 || parsed > RANK_MAX)
            return false;
        target = parsed;
        return true;
    }

    template <> bool set(const std::string& value, File& target) {
        std::stringstream ss(value);
        ss >> std::ws;
        File parsed;
        if (std::isdigit(ss.peek()))
        {
            int i;
            ss >> i;
            parsed = File(i - 1);
        }
        else
        {
            char c;
            ss >> c;
            parsed = File(c - 'a');
        }
        if (ss.fail() || !only_trailing_space(ss) || parsed < FILE_A || parsed > FILE_MAX)
            return false;
        target = parsed;
        return true;
    }

    template <> bool set(const std::string& value, std::string& target) {
        target = value;
        return true;
    }

    template <> bool set(const std::string& value, bool& target) {
        target = value == "true";
        return value == "true" || value == "false";
    }

    template <> bool set(const std::string& value, Value& target) {
        target =  value == "win"  ? VALUE_MATE
                : value == "loss" ? -VALUE_MATE
                : value == "draw" ? VALUE_DRAW
                : VALUE_NONE;
        return value == "win" || value == "loss" || value == "draw" || value == "none";
    }

    template <> bool set(const std::string& value, CapturingRule& target) {
        target = value == "out" ? MOVE_OUT
                : value == "hand" ? HAND
                : value == "prison" ? PRISON
                : MOVE_OUT;
        return value == "out" || value == "hand" || value == "prison";
    }

    template <> bool set(const std::string& value, MaterialCounting& target) {
        target =  value == "janggi"  ? JANGGI_MATERIAL
                : value == "unweighted" ? UNWEIGHTED_MATERIAL
                : value == "whitedrawodds" ? WHITE_DRAW_ODDS
                : value == "blackdrawodds" ? BLACK_DRAW_ODDS
                : NO_MATERIAL_COUNTING;
        return   value == "janggi" || value == "unweighted"
              || value == "whitedrawodds" || value == "blackdrawodds" || value == "none";
    }

    template <> bool set(const std::string& value, CountingRule& target) {
        target =  value == "makruk"  ? MAKRUK_COUNTING
                : value == "cambodian" ? CAMBODIAN_COUNTING
                : value == "asean" ? ASEAN_COUNTING
                : NO_COUNTING;
        return value == "makruk" || value == "cambodian" || value == "asean" || value == "none";
    }

    template <> bool set(const std::string& value, ChasingRule& target) {
        target =  value == "axf"  ? AXF_CHASING
                : NO_CHASING;
        return value == "axf" || value == "none";
    }

    template <> bool set(const std::string& value, EnclosingRule& target) {
        target =  value == "reversi"  ? REVERSI
                : value == "ataxx" ? ATAXX
                : value == "quadwrangle" ? QUADWRANGLE
                : value == "snort" ? SNORT
                : value == "anyside" ? ANYSIDE
                : value == "top" ? TOP
                : NO_ENCLOSING;
        return value == "reversi" || value == "ataxx" || value == "quadwrangle" || value =="snort" || value =="anyside" || value =="top" || value == "none";
    }

    template <> bool set(const std::string& value, WallingRule& target) {
        target =  value == "arrow"  ? ARROW
                : value == "duck" ? DUCK
                : value == "edge" ? EDGE
                : value == "past" ? PAST
                : value == "static" ? STATIC
                : NO_WALLING;
        return value == "arrow" || value == "duck" || value == "edge" || value =="past" || value == "static" || value == "none";
    }

    template <> bool set(const std::string& value, PointsRule& target) {
        target =  value == "us" ? POINTS_US
                : value == "them" ? POINTS_THEM
                : value == "owner" ? POINTS_OWNER
                : value == "non-owner" ? POINTS_NON_OWNER
                : POINTS_NONE;
        return value == "us" || value == "them" || value =="owner" || value =="non-owner" || value =="none";
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
        if (ss.fail())
            return false;
        target = parsed;
        return true;
    }

    template <> bool set(const std::string& value, PieceTypeBitboardGroup& target) {
        size_t i;
        int ParserState = -1;
        int RankNum = 0;
        int FileNum = 0;
        char PieceChar = 0;
        Bitboard board = 0x00;
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
                    target = PieceTypeBitboardGroup();
                    return true;
                }
                ParserState = 0;
            }
            if (ParserState == 0)  // Find piece type character
            {
                if (ch >= 'A' && ch <= 'Z')
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
                        target.set(PieceChar, target.boardOfPiece(PieceChar) | board);
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
        if (valid)
            target = parsed;
        return valid;
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

    void parse_hostage_exchanges(Variant *v, const std::string &map, bool DoCheck) {
        bool readPiece = true;
        size_t idx = -1;
        PieceSet mask = NO_PIECE_SET;
        for (size_t i = 0; i < map.size(); ++i) {
            char token = map[i];
            if (token == ' ') {
                if (!readPiece) {
                    v->hostageExchange[idx] = mask;
                    readPiece = true;
                }
                continue;
            }
            if (readPiece) {
                mask = NO_PIECE_SET;
                idx = v->pieceToChar.find(std::toupper(static_cast<unsigned char>(token)));
                if (idx == std::string::npos) {
                    if (DoCheck) {
                        std::cerr << "hostageExchange - Invalid piece type: " << token << std::endl;
                    }
                    return;
                }
                readPiece = false;
            } else if (token == ':') {
                if (mask != NO_PIECE_SET) {
                    if (DoCheck) {
                        std::cerr << "hostageExchange - Invalid syntax: " << map << std::endl;
                    }
                    return;
                }
            } else {
                size_t idx2 = v->pieceToChar.find(std::toupper(static_cast<unsigned char>(token)));
                if (idx2 == std::string::npos) {
                    if (DoCheck) {
                        std::cerr << "hostageExchange - Invalid hostage piece type: " << token << std::endl;
                    }
                    return;
                }
                mask = mask | PieceType(idx2);
            }
        }
        if (!readPiece && idx != std::string::npos)
            v->hostageExchange[idx] = mask;
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
                                  : std::is_same<T, WallingRule>() ? "WallingRule"
                                  : std::is_same<T, std::vector<int>>() ? "vector<int>"
                                  : typeid(T).name();
            std::cerr << key << " - Invalid value " << it->second << " for type " << typeName << std::endl;
        }
        return valid;
    }
    return false;
}

template <bool DoCheck>
template <bool Current, class T> bool VariantParser<DoCheck>::parse_attribute(const std::string& key, T& target, const std::string& pieceToChar) {
    const auto& it = config.find(key);
    if (it != config.end())
    {
        T parsedTarget = T();
        char token;
        size_t idx = std::string::npos;
        std::stringstream ss(it->second);
        while (ss >> token && token != '-' && (idx = token == '*' ? size_t(ALL_PIECES) : pieceToChar.find(std::toupper(static_cast<unsigned char>(token)))) != std::string::npos)
            set(PieceType(idx), parsedTarget);
        if (DoCheck && idx == std::string::npos && token != '-')
            std::cerr << key << " - Invalid piece type: " << token << std::endl;
        else if ((idx != std::string::npos || token == '-') && !only_trailing_space(ss))
        {
            if (DoCheck)
                std::cerr << key << " - Invalid trailing characters." << std::endl;
            return false;
        }

        if (idx != std::string::npos || token == '-')
        {
            target = parsedTarget;
            return true;
        }
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
template <typename T>
void VariantParser<DoCheck>::parse_both_colors(const std::string& key, T& target) {
    parse_attribute(key, target[WHITE]);
    parse_attribute(key, target[BLACK]);
}

template <bool DoCheck>
template <typename T>
void VariantParser<DoCheck>::parse_both_colors_piece(const std::string& key, T& target, const std::string& pieceToChar) {
    parse_attribute(key, target[WHITE], pieceToChar);
    parse_attribute(key, target[BLACK], pieceToChar);
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
            if (std::isalpha(static_cast<unsigned char>(keyValue->second.at(0))))
                v->add_piece(pt, keyValue->second.at(0));
            else
            {
                if (DoCheck && keyValue->second.at(0) != '-')
                    std::cerr << name << " - Invalid letter: " << keyValue->second.at(0) << std::endl;
                v->remove_piece(pt);
            }
            // betza
            if (is_custom(pt))
            {
                if (keyValue->second.size() > 1)
                {
                    v->customPiece[pt - CUSTOM_PIECES] = keyValue->second.substr(2);
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
            else if (pt == KING)
            {
                if (keyValue->second.size() > 1)
                {
                    // custom royal piece
                    v->add_piece(CUSTOM_PIECES_ROYAL, keyValue->second.at(0));
                    v->customPiece[CUSTOM_PIECES_ROYAL - CUSTOM_PIECES] = keyValue->second.substr(2);
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
            char token, sep = 0;
            size_t idx = std::string::npos;
            bool parseError = false;
            int parsedValue = 0;
            std::stringstream ss(pv->second);
            while (ss >> token)
            {
                idx = v->pieceToChar.find(std::toupper(static_cast<unsigned char>(token)));
                if (idx == std::string::npos)
                    break;
                if (!(ss >> sep) || sep != ':' || !(ss >> parsedValue))
                {
                    parseError = true;
                    break;
                }
                v->pieceValue[phase][idx] = parsedValue;
            }
            if (DoCheck && idx == std::string::npos)
                std::cerr << optionName << " - Invalid piece type: " << token << std::endl;
            else if (DoCheck && (parseError || !(ss >> std::ws).eof()))
                std::cerr << optionName << " - Invalid piece value for type: " << v->pieceToChar[idx] << std::endl;
        }
    }

    // piece points (for games of points, not evaluation)
    const auto& pv = config.find("piecePoints");
    if (pv != config.end())
    {
        char token = '\0', sep = 0;
        size_t idx = std::string::npos;
        int parsedPoints = 0;
        bool parseError = false;
        bool sawToken = false;
        std::stringstream ss(pv->second);
        while (ss >> token)
        {
            sawToken = true;
            idx = v->pieceToChar.find(std::toupper(static_cast<unsigned char>(token)));
            if (idx == std::string::npos)
                break;
            if (!(ss >> sep) || sep != ':' || !(ss >> parsedPoints))
            {
                parseError = true;
                break;
            }
            if (parsedPoints < 0) {
                if (DoCheck)
                    std::cerr << "piecePoints - Negative values are not allowed for type: " << v->pieceToChar[idx] << std::endl;
                parsedPoints = 0;
            }
            if (parsedPoints > MAX_PIECE_POINTS) {
                if (DoCheck)
                    std::cerr << "piecePoints - Value exceeds max " << MAX_PIECE_POINTS
                              << " for type: " << v->pieceToChar[idx] << ". Clamping." << std::endl;
                parsedPoints = MAX_PIECE_POINTS;
            }
            v->piecePoints[idx] = parsedPoints;
        }
        if (DoCheck && sawToken && idx == std::string::npos)
            std::cerr << "piecePoints - Invalid piece type: " << token << std::endl;
        else if (DoCheck && sawToken && idx != std::string::npos && (parseError || !(ss >> std::ws).eof()))
            std::cerr << "piecePoints - Invalid piece points for type: " << v->pieceToChar[idx] << std::endl;
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
    parse_attribute<false>("whiteFlag", v->flagRegion[WHITE]);
    parse_attribute<false>("blackFlag", v->flagRegion[BLACK]);
    parse_attribute<false>("castlingRookPiece", v->castlingRookPieces[WHITE], v->pieceToChar);
    parse_attribute<false>("castlingRookPiece", v->castlingRookPieces[BLACK], v->pieceToChar);
    parse_attribute<false>("whiteDropRegion", v->dropRegion[WHITE]);
    parse_attribute<false>("blackDropRegion", v->dropRegion[BLACK]);

    bool dropOnTop = false;
    parse_attribute<false>("dropOnTop", dropOnTop);
    if (dropOnTop) v->enclosingDrop=TOP;

    // Parse aliases
    parse_both_colors_piece("pawnTypes", v->mainPromotionPawnType, v->pieceToChar);
    parse_both_colors_piece("pawnTypes", v->promotionPawnTypes, v->pieceToChar);
    parse_both_colors_piece("pawnTypes", v->enPassantTypes, v->pieceToChar);
    parse_both_colors_piece("pawnTypes", v->nMoveRuleTypes, v->pieceToChar);
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
    parse_attribute("startFen", v->startFen);
    parse_attribute("promotionRegionWhite", v->promotionRegion[WHITE]);
    parse_attribute("promotionRegionBlack", v->promotionRegion[BLACK]);
    parse_attribute("promotionRegion", v->promotionRegion[WHITE]);
    parse_attribute("promotionRegion", v->promotionRegion[BLACK]);
    parse_attribute("mandatoryPromotionRegionWhite", v->mandatoryPromotionRegion[WHITE]);
    parse_attribute("mandatoryPromotionRegionBlack", v->mandatoryPromotionRegion[BLACK]);
    parse_attribute("mandatoryPromotionRegion", v->mandatoryPromotionRegion[WHITE]);
    parse_attribute("mandatoryPromotionRegion", v->mandatoryPromotionRegion[BLACK]);
    parse_attribute("pieceSpecificPromotionRegion", v->pieceSpecificPromotionRegion);
    if (!require_attribute(v->pieceSpecificPromotionRegion, "whitePiecePromotionRegion", v->whitePiecePromotionRegion)
        || !require_attribute(v->pieceSpecificPromotionRegion, "blackPiecePromotionRegion", v->blackPiecePromotionRegion))
        return false;
    // Take the first promotionPawnTypes as the main promotionPawnType
    parse_both_colors_piece("promotionPawnTypes", v->mainPromotionPawnType, v->pieceToChar);
    parse_both_colors_piece("promotionPawnTypes", v->promotionPawnTypes, v->pieceToChar);
    parse_attribute("promotionPawnTypesWhite", v->mainPromotionPawnType[WHITE], v->pieceToChar);
    parse_attribute("promotionPawnTypesBlack", v->mainPromotionPawnType[BLACK], v->pieceToChar);
    parse_attribute("promotionPawnTypesWhite", v->promotionPawnTypes[WHITE], v->pieceToChar);
    parse_attribute("promotionPawnTypesBlack", v->promotionPawnTypes[BLACK], v->pieceToChar);
    parse_both_colors_piece("promotionPieceTypes", v->promotionPieceTypes, v->pieceToChar);
    parse_attribute("promotionPieceTypesWhite", v->promotionPieceTypes[WHITE], v->pieceToChar);
    parse_attribute("promotionPieceTypesBlack", v->promotionPieceTypes[BLACK], v->pieceToChar);
    parse_attribute("sittuyinPromotion", v->sittuyinPromotion);
    parse_attribute("promotionSteal", v->promotionSteal);
    parse_attribute("promotionRequireInHand", v->promotionRequireInHand);
    parse_attribute("promotionConsumeInHand", v->promotionConsumeInHand);
    // promotion limit
    const auto& it_prom_limit = config.find("promotionLimit");
    if (it_prom_limit != config.end())
    {
        char token = '\0', sep = 0;
        size_t idx = std::string::npos;
        bool parseError = false;
        int parsedLimit = 0;
        bool sawToken = false;
        std::stringstream ss(it_prom_limit->second);
        while (ss >> token)
        {
            sawToken = true;
            idx = v->pieceToChar.find(std::toupper(static_cast<unsigned char>(token)));
            if (idx == std::string::npos)
                break;
            if (!(ss >> sep) || sep != ':' || !(ss >> parsedLimit))
            {
                parseError = true;
                break;
            }
            v->promotionLimit[idx] = parsedLimit;
        }
        if (DoCheck && sawToken && idx == std::string::npos)
            std::cerr << "promotionLimit - Invalid piece type: " << token << std::endl;
        else if (DoCheck && sawToken && idx != std::string::npos && (parseError || !(ss >> std::ws).eof()))
            std::cerr << "promotionLimit - Invalid piece count for type: " << v->pieceToChar[idx] << std::endl;
    }
    // promoted piece types
    const auto& it_prom_pt = config.find("promotedPieceType");
    if (it_prom_pt != config.end())
    {
        char token, sep = 0;
        size_t idx = std::string::npos, idx2 = std::string::npos;
        bool parseError = false;
        std::stringstream ss(it_prom_pt->second);
        while (ss >> token)
        {
            idx = v->pieceToChar.find(std::toupper(static_cast<unsigned char>(token)));
            if (idx == std::string::npos)
                break;
            if (!(ss >> sep) || sep != ':' || !(ss >> token))
            {
                parseError = true;
                break;
            }
            idx2 = (token == '-' ? 0 : v->pieceToChar.find(std::toupper(static_cast<unsigned char>(token))));
            if (idx2 == std::string::npos)
                break;
            v->promotedPieceType[idx] = PieceType(idx2);
        }
        if (DoCheck && (idx == std::string::npos || idx2 == std::string::npos))
            std::cerr << "promotedPieceType - Invalid piece type: " << token << std::endl;
        else if (DoCheck && (parseError || !(ss >> std::ws).eof()))
            std::cerr << "promotedPieceType - Invalid syntax." << std::endl;
    }
    // priority drops
    const auto& it_pr_drop = config.find("priorityDropTypes");
    if (it_pr_drop != config.end())
    {
        char token = '\0';
        size_t idx = std::string::npos;
        bool parsedPriorityDrops[PIECE_TYPE_NB];
        bool sawToken = false;
        std::copy(std::begin(v->isPriorityDrop), std::end(v->isPriorityDrop), std::begin(parsedPriorityDrops));
        std::stringstream ss(it_pr_drop->second);
        while (ss >> token)
        {
            sawToken = true;
            if (token == '-')
                break;
            idx = v->pieceToChar.find(std::toupper(static_cast<unsigned char>(token)));
            if (idx == std::string::npos)
                break;
            parsedPriorityDrops[PieceType(idx)] = true;
        }
        if (DoCheck && sawToken && idx == std::string::npos && token != '-')
            std::cerr << "priorityDropTypes - Invalid piece type: " << token << std::endl;
        else if (sawToken && (idx != std::string::npos || token == '-') && !only_trailing_space(ss))
        {
            if (DoCheck)
                std::cerr << "priorityDropTypes - Invalid trailing characters." << std::endl;
        }
        else if (sawToken && (idx != std::string::npos || token == '-'))
            std::copy(std::begin(parsedPriorityDrops), std::end(parsedPriorityDrops), std::begin(v->isPriorityDrop));
    }
    parse_attribute("piecePromotionOnCapture", v->piecePromotionOnCapture);
    parse_attribute("mandatoryPawnPromotion", v->mandatoryPawnPromotion);
    parse_attribute("mandatoryPiecePromotion", v->mandatoryPiecePromotion);
    parse_attribute("pieceDemotion", v->pieceDemotion);
    parse_attribute("blastOnCapture", v->blastOnCapture);
    parse_attribute("blastOnMove", v->blastOnMove);
    parse_attribute("blastPromotion", v->blastPromotion);
    parse_attribute("blastDiagonals", v->blastDiagonals);
    parse_attribute("blastCenter", v->blastCenter);
    parse_attribute("blastImmuneTypes", v->blastImmuneTypes, v->pieceToChar);
    parse_attribute("mutuallyImmuneTypes", v->mutuallyImmuneTypes, v->pieceToChar);
    parse_attribute("deathOnCaptureTypes", v->deathOnCaptureTypes, v->pieceToChar);
    parse_attribute("mutuallyHopIllegalTypes", v->mutuallyHopIllegalTypes, v->pieceToChar);
    auto parse_capture_map = [&](const std::string& key, bool allow) {
        const auto& it = config.find(key);
        if (it == config.end())
            return;

        std::string entry;
        std::stringstream ss(it->second);
        while (ss >> entry) {
            size_t sep = entry.find(':');
            if (sep == std::string::npos || sep == 0 || sep + 1 >= entry.size()) {
                if (DoCheck)
                    std::cerr << key << " - Invalid mapping token: " << entry << std::endl;
                continue;
            }

            std::string attackers = entry.substr(0, sep);
            std::string targets = entry.substr(sep + 1);

            PieceSet attackerSet = NO_PIECE_SET;
            if (attackers == "*") {
                attackerSet = v->pieceTypes;
            } else {
                for (char a : attackers) {
                    size_t idx = v->pieceToChar.find(std::toupper(static_cast<unsigned char>(a)));
                    if (idx == std::string::npos || idx >= PIECE_TYPE_NB) {
                        if (DoCheck)
                            std::cerr << key << " - Invalid attacker piece type: " << a << std::endl;
                        continue;
                    }
                    attackerSet |= piece_set(PieceType(idx));
                }
            }

            PieceSet targetSet = NO_PIECE_SET;
            if (targets != "-") {
                if (targets == "*") {
                    targetSet = v->pieceTypes;
                } else {
                    for (char t : targets) {
                        size_t idx = v->pieceToChar.find(std::toupper(static_cast<unsigned char>(t)));
                        if (idx == std::string::npos || idx >= PIECE_TYPE_NB) {
                            if (DoCheck)
                                std::cerr << key << " - Invalid target piece type: " << t << std::endl;
                            continue;
                        }
                        targetSet |= piece_set(PieceType(idx));
                    }
                }
            }

            if (!attackerSet || !targetSet)
                continue;

            for (PieceSet ps = attackerSet; ps; ) {
                PieceType attacker = pop_lsb(ps);
                if (allow)
                    v->captureForbidden[attacker] &= ~targetSet;
                else
                    v->captureForbidden[attacker] |= targetSet;
            }
        }
    };
    parse_capture_map("captureForbidden", false);
    parse_capture_map("captureAllowed", true);
    parse_attribute("petrifyOnCaptureTypes", v->petrifyOnCaptureTypes, v->pieceToChar);
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
    parse_attribute("doubleStepRegionWhite", v->doubleStepRegion[WHITE]);
    parse_attribute("doubleStepRegionBlack", v->doubleStepRegion[BLACK]);
    parse_attribute("pieceSpecificDoubleStepRegion", v->pieceSpecificDoubleStepRegion);
    if (!require_attribute(v->pieceSpecificDoubleStepRegion, "whitePieceDoubleStepRegion", v->whitePieceDoubleStepRegion)
        || !require_attribute(v->pieceSpecificDoubleStepRegion, "blackPieceDoubleStepRegion", v->blackPieceDoubleStepRegion))
        return false;
    parse_attribute("pieceSpecificTripleStepRegion", v->pieceSpecificTripleStepRegion);
    if (!require_attribute(v->pieceSpecificTripleStepRegion, "whitePieceTripleStepRegion", v->whitePieceTripleStepRegion)
        || !require_attribute(v->pieceSpecificTripleStepRegion, "blackPieceTripleStepRegion", v->blackPieceTripleStepRegion))
        return false;
    parse_attribute("tripleStepRegionWhite", v->tripleStepRegion[WHITE]);
    parse_attribute("tripleStepRegionBlack", v->tripleStepRegion[BLACK]);
    parse_both_colors("enPassantRegion", v->enPassantRegion);
    parse_attribute("enPassantRegionWhite", v->enPassantRegion[WHITE]);
    parse_attribute("enPassantRegionBlack", v->enPassantRegion[BLACK]);
    parse_both_colors_piece("enPassantTypes", v->enPassantTypes, v->pieceToChar);
    parse_attribute("enPassantTypesWhite", v->enPassantTypes[WHITE], v->pieceToChar);
    parse_attribute("enPassantTypesBlack", v->enPassantTypes[BLACK], v->pieceToChar);
    parse_attribute("castling", v->castling);
    parse_attribute("castlingDroppedPiece", v->castlingDroppedPiece);
    parse_attribute("castlingForbiddenPlies", v->castlingForbiddenPlies);
    parse_attribute("castlingKingsideFile", v->castlingKingsideFile);
    parse_attribute("castlingQueensideFile", v->castlingQueensideFile);
    parse_attribute("castlingRank", v->castlingRank);
    parse_attribute("castlingKingFile", v->castlingKingFile);
    parse_both_colors_piece("castlingKingPiece", v->castlingKingPiece, v->pieceToChar);
    parse_attribute("castlingKingPieceWhite", v->castlingKingPiece[WHITE], v->pieceToChar);
    parse_attribute("castlingKingPieceBlack", v->castlingKingPiece[BLACK], v->pieceToChar);
    parse_attribute("castlingRookKingsideFile", v->castlingRookKingsideFile);
    parse_attribute("castlingRookQueensideFile", v->castlingRookQueensideFile);
    parse_both_colors_piece("castlingRookPieces", v->castlingRookPieces, v->pieceToChar);
    parse_attribute("castlingRookPiecesWhite", v->castlingRookPieces[WHITE], v->pieceToChar);
    parse_attribute("castlingRookPiecesBlack", v->castlingRookPieces[BLACK], v->pieceToChar);
    parse_attribute("oppositeCastling", v->oppositeCastling);
    parse_attribute("checking", v->checking);
    parse_attribute("allowChecks", v->allowChecks);
    parse_attribute("royalPieceNoThroughCheck", v->royalPieceNoThroughCheck);
    parse_attribute("dropChecks", v->dropChecks);
    parse_attribute("dropMates", v->dropMates);
    parse_attribute("mustCapture", v->mustCapture);
    parse_attribute("mustCaptureEnPassant", v->mustCaptureEnPassant);
    parse_attribute("mustCaptureWhite", v->mustCaptureByColor[WHITE]);
    parse_attribute("mustCaptureBlack", v->mustCaptureByColor[BLACK]);
    parse_attribute("rifleCapture", v->rifleCapture);
    parse_attribute("selfCapture", v->selfCapture);
    parse_attribute("capturerDiesOnCapture", v->capturerDiesOnCapture);
    parse_attribute("capturerDiesOnSameTypeCapture", v->capturerDiesOnSameTypeCapture);
    parse_attribute("capturerDiesExemptTypes", v->capturerDiesExemptTypes, v->pieceToChar);
    parse_attribute("capturerDiesExemptPawns", v->capturerDiesExemptPawns);
    parse_attribute("captureMorph", v->captureMorph);
    parse_attribute("rexExclusiveMorph", v->rexExclusiveMorph);
    parse_attribute("mustDrop", v->mustDrop);
    parse_attribute("mustDropWhite", v->mustDropByColor[WHITE]);
    parse_attribute("mustDropBlack", v->mustDropByColor[BLACK]);
    parse_attribute("mustDropType", v->mustDropType, v->pieceToChar);
    parse_attribute("mustDropTypeWhite", v->mustDropTypeByColor[WHITE], v->pieceToChar);
    parse_attribute("mustDropTypeBlack", v->mustDropTypeByColor[BLACK], v->pieceToChar);
    parse_attribute("dropKingLast", v->dropKingLast);
    parse_attribute("openingSelfRemoval", v->openingSelfRemoval);
    parse_attribute("openingSelfRemovalAdjacentToLast", v->openingSelfRemovalAdjacentToLast);
    parse_both_colors("openingSelfRemovalRegion", v->openingSelfRemovalRegion);
    parse_attribute("openingSelfRemovalRegionWhite", v->openingSelfRemovalRegion[WHITE]);
    parse_attribute("openingSelfRemovalRegionBlack", v->openingSelfRemovalRegion[BLACK]);
    parse_attribute("pieceDrops", v->pieceDrops);
    parse_attribute("virtualDrops", v->virtualDrops);
    const auto& it_virtual_drop_limit = config.find("virtualDropLimit");
    if (it_virtual_drop_limit != config.end())
    {
        char token = '\0', sep = 0;
        size_t idx = std::string::npos;
        int limit = 0;
        int parsedLimits[PIECE_TYPE_NB];
        std::copy(std::begin(v->virtualDropLimit), std::end(v->virtualDropLimit), std::begin(parsedLimits));
        bool parsedEnabled = v->virtualDropLimitEnabled;
        bool parseError = false;
        bool sawToken = false;
        std::stringstream ss(it_virtual_drop_limit->second);
        while (ss >> token)
        {
            sawToken = true;
            idx = v->pieceToChar.find(std::toupper(static_cast<unsigned char>(token)));
            if (idx == std::string::npos)
                break;
            if (idx >= PIECE_TYPE_NB || !(ss >> sep) || sep != ':' || !(ss >> limit))
            {
                parseError = true;
                break;
            }
            if (limit < 0)
            {
                if (DoCheck)
                    std::cerr << "virtualDropLimit - Invalid negative value for type: " << v->pieceToChar[idx] << std::endl;
                return false;
            }
            parsedLimits[PieceType(idx)] = limit;
            parsedEnabled = true;
        }
        if (DoCheck && sawToken && idx == std::string::npos)
            std::cerr << "virtualDropLimit - Invalid piece type: " << token << std::endl;
        else if (DoCheck && sawToken && idx != std::string::npos && (parseError || !(ss >> std::ws).eof()))
            std::cerr << "virtualDropLimit - Invalid syntax." << std::endl;
        else if (sawToken && idx != std::string::npos)
        {
            std::copy(std::begin(parsedLimits), std::end(parsedLimits), std::begin(v->virtualDropLimit));
            v->virtualDropLimitEnabled = parsedEnabled;
        }
    }
    parse_attribute("dropLoop", v->dropLoop);

    bool capturesToHand = false;
    if (parse_attribute<false>("capturesToHand", capturesToHand)) {
        v->captureType = capturesToHand ? HAND : MOVE_OUT;
    }

    parse_attribute("captureType", v->captureType);
    // hostage price
    const auto& it_host_p = config.find("hostageExchange");
    if (it_host_p != config.end()) {
        parse_hostage_exchanges(v, it_host_p->second, DoCheck);
    }
    parse_attribute("prisonPawnPromotion", v->prisonPawnPromotion);
    parse_attribute("firstRankPawnDrops", v->firstRankPawnDrops);
    parse_attribute("promotionZonePawnDrops", v->promotionZonePawnDrops);
    parse_attribute("enclosingDrop", v->enclosingDrop);
    parse_attribute("enclosingDropStart", v->enclosingDropStart);
    parse_attribute("dropRegionWhite", v->dropRegion[WHITE]);
    parse_attribute("dropRegionBlack", v->dropRegion[BLACK]);
    parse_attribute("pieceSpecificDropRegion", v->pieceSpecificDropRegion);
    if (!require_attribute(v->pieceSpecificDropRegion, "whitePieceDropRegion", v->whitePieceDropRegion)
        || !require_attribute(v->pieceSpecificDropRegion, "blackPieceDropRegion", v->blackPieceDropRegion))
        return false;
    parse_attribute("sittuyinRookDrop", v->sittuyinRookDrop);
    parse_attribute("dropOppositeColoredBishop", v->dropOppositeColoredBishop);
    parse_attribute("dropPromoted", v->dropPromoted);
    parse_attribute("dropNoDoubled", v->dropNoDoubled, v->pieceToChar);
    parse_attribute("dropNoDoubledCount", v->dropNoDoubledCount);
    parse_attribute("freeDrops", v->freeDrops);
    parse_attribute("payPointsToDrop", v->payPointsToDrop);
    parse_attribute("potions", v->potions);
    parse_attribute("freezePotion", v->potionPiece[Variant::POTION_FREEZE], v->pieceToChar);
    parse_attribute("jumpPotion", v->potionPiece[Variant::POTION_JUMP], v->pieceToChar);
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
    parse_attribute("wallingWhite", v->wallingSide[WHITE]);
    parse_attribute("wallingBlack", v->wallingSide[BLACK]);
    parse_attribute("wallingRegionWhite", v->wallingRegion[WHITE]);
    parse_attribute("wallingRegionBlack", v->wallingRegion[BLACK]);
    parse_attribute("wallingRegion", v->wallingRegion[WHITE]);
    parse_attribute("wallingRegion", v->wallingRegion[BLACK]);
    parse_attribute("wallOrMove", v->wallOrMove);
    parse_attribute("seirawanGating", v->seirawanGating);
    parse_attribute("commitGates", v->commitGates);
    parse_attribute("jumpCaptureTypes", v->jumpCaptureTypes, v->pieceToChar);
    if (v->jumpCaptureTypes & PAWN)
    {
        if (DoCheck)
            std::cerr << "jumpCaptureTypes - PAWN is not supported for jump captures and will be ignored." << std::endl;
        v->jumpCaptureTypes &= ~piece_set(PAWN);
    }
    parse_attribute("forcedJumpContinuation", v->forcedJumpContinuation);
    parse_attribute("forcedJumpSameDirection", v->forcedJumpSameDirection);
    parse_attribute("cambodianMoves", v->cambodianMoves);
    parse_attribute("diagonalLines", v->diagonalLines);
    parse_both_colors("pass", v->pass);
    parse_attribute("passWhite", v->pass[WHITE]);
    parse_attribute("passBlack", v->pass[BLACK]);
    parse_both_colors("passOnStalemate", v->passOnStalemate);
    parse_attribute("passOnStalemateWhite", v->passOnStalemate[WHITE]);
    parse_attribute("passOnStalemateBlack", v->passOnStalemate[BLACK]);
    parse_attribute("passUntilSetup", v->passUntilSetup);
    parse_attribute("multimoves", v->multimoves);
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
    parse_both_colors_piece("nMoveRuleTypes", v->nMoveRuleTypes, v->pieceToChar);
    parse_attribute("nMoveRuleTypesWhite", v->nMoveRuleTypes[WHITE], v->pieceToChar);
    parse_attribute("nMoveRuleTypesBlack", v->nMoveRuleTypes[BLACK], v->pieceToChar);
    parse_attribute("nMoveRule", v->nMoveRule);
    parse_attribute("nMoveRuleImmediate", v->nMoveRuleImmediate);
    parse_attribute("nMoveHardLimitRule", v->nMoveHardLimitRule);
    parse_attribute("nMoveHardLimitRuleValue", v->nMoveHardLimitRuleValue);
    parse_attribute("nFoldRule", v->nFoldRule);
    parse_attribute("nFoldRuleImmediate", v->nFoldRuleImmediate);
    parse_attribute("nFoldValue", v->nFoldValue);
    parse_attribute("nFoldValueAbsolute", v->nFoldValueAbsolute);
    parse_attribute("perpetualCheckIllegal", v->perpetualCheckIllegal);
    parse_attribute("moveRepetitionIllegal", v->moveRepetitionIllegal);
    parse_attribute("chasingRule", v->chasingRule);
    parse_attribute("stalemateValue", v->stalemateValue);
    parse_attribute("stalematePieceCount", v->stalematePieceCount);
    parse_attribute("checkmateValue", v->checkmateValue);
    parse_attribute("shogiPawnDropMateIllegal", v->shogiPawnDropMateIllegal);
    parse_attribute("shatarMateRule", v->shatarMateRule);
    parse_attribute("bikjangRule", v->bikjangRule);
    parse_attribute("pseudoRoyalTypes", v->pseudoRoyalTypes, v->pieceToChar);
    parse_attribute("pseudoRoyalCount", v->pseudoRoyalCount);
    parse_attribute("antiRoyalTypes", v->antiRoyalTypes, v->pieceToChar);
    parse_attribute("antiRoyalCount", v->antiRoyalCount);
    parse_attribute("extinctionValue", v->extinctionValue);
    parse_attribute("extinctionClaim", v->extinctionClaim);
    parse_attribute("extinctionPseudoRoyal", v->extinctionPseudoRoyal);
    parse_attribute("dupleCheck", v->dupleCheck);
    // extinction piece types
    parse_attribute("extinctionPieceTypes", v->extinctionPieceTypes, v->pieceToChar);
    parse_attribute("extinctionPieceCount", v->extinctionPieceCount);
    parse_attribute("extinctionOpponentPieceCount", v->extinctionOpponentPieceCount);

    // Backward compatibility for legacy extinctionPseudoRoyal configs.
    if (v->extinctionPseudoRoyal && !v->pseudoRoyalTypes)
    {
        v->pseudoRoyalTypes = v->extinctionPieceTypes;
        v->pseudoRoyalCount = v->extinctionPieceCount + 1;
    }
    parse_both_colors_piece("flagPiece", v->flagPiece, v->pieceToChar);
    parse_attribute("flagPieceWhite", v->flagPiece[WHITE], v->pieceToChar);
    parse_attribute("flagPieceBlack", v->flagPiece[BLACK], v->pieceToChar);
    parse_both_colors("flagRegion", v->flagRegion);
    parse_attribute("flagRegionWhite", v->flagRegion[WHITE]);
    parse_attribute("flagRegionBlack", v->flagRegion[BLACK]);
    parse_attribute("flagPieceCount", v->flagPieceCount);
    parse_attribute("flagPieceBlockedWin", v->flagPieceBlockedWin);
    parse_attribute("flagMove", v->flagMove);
    parse_attribute("flagPieceSafe", v->flagPieceSafe);
    parse_attribute("checkCounting", v->checkCounting);
    parse_attribute("connectN", v->connectN);
    parse_attribute("connectPieceTypes", v->connectPieceTypes, v->pieceToChar);
    parse_attribute("connectGoalByType", v->connectGoalByType);
    parse_attribute("connectPieceGoalWhite", v->connectPieceGoal[WHITE]);
    parse_attribute("connectPieceGoalBlack", v->connectPieceGoal[BLACK]);
    parse_attribute("connectHorizontal", v->connectHorizontal);
    parse_attribute("connectVertical", v->connectVertical);
    parse_attribute("connectDiagonal", v->connectDiagonal);
    parse_attribute("connect3D", v->connect3D);
    parse_attribute("connect4D", v->connect4D);
    parse_attribute("connectRegion1White", v->connectRegion1[WHITE]);
    parse_attribute("connectRegion2White", v->connectRegion2[WHITE]);
    parse_attribute("connectRegion1Black", v->connectRegion1[BLACK]);
    parse_attribute("connectRegion2Black", v->connectRegion2[BLACK]);
    parse_attribute("connectNxN", v->connectNxN);
    parse_attribute("collinearN", v->collinearN);
    parse_attribute("connectGroup", v->connectGroup);
    parse_attribute("connectValue", v->connectValue);
    parse_attribute("materialCounting", v->materialCounting);
    parse_attribute("adjudicateFullBoard", v->adjudicateFullBoard);
    parse_attribute("countingRule", v->countingRule);
    parse_attribute("castlingWins", v->castlingWins);
    parse_attribute("pointsCounting", v->pointsCounting);
    parse_attribute("pointsRuleCaptures", v->pointsRuleCaptures);
    parse_attribute("pointsGoal", v->pointsGoal);
    parse_attribute("pointsGoalValue", v->pointsGoalValue);
    parse_attribute("pointsGoalSimulValue", v->pointsGoalSimulValue);
    if (v->payPointsToDrop)
        v->pointsCounting = true;

    // Report invalid options
    if (DoCheck)
    {
        const std::set<std::string>& parsedKeys = config.get_consumed_keys();
        for (const auto& it : config)
            if (parsedKeys.find(it.first) == parsedKeys.end())
                std::cerr << "Invalid option: " << it.first << std::endl;
    }
    return true;
}

template <bool DoCheck>
bool VariantParser<DoCheck>::check_consistency(Variant* v) {
    // Check for limitations that would cause undefined behavior or crashes
    if ((v->pieceDrops || v->freeDrops) && v->wallingRule != NO_WALLING)
    {
        std::cerr << "pieceDrops and any walling are incompatible." << std::endl;
        return false;
    }
    if (v->wallingRule != NO_WALLING && v->seirawanGating)
    {
        std::cerr << "wallingRule and seirawanGating are incompatible." << std::endl;
        return false;
    }
    if (v->wallingRule != NO_WALLING && v->potions)
    {
        std::cerr << "wallingRule and potions are incompatible." << std::endl;
        return false;
    }
    if (v->wallingRule == DUCK && v->petrifyOnCaptureTypes)
    {
        std::cerr << "wallingRule=duck and petrifyOnCaptureTypes are incompatible." << std::endl;
        return false;
    }
    if ((v->pieceTypes & KING) && v->wallingRule == DUCK)
    {
        std::cerr << "Can not use kings with wallingRule = duck." << std::endl;
        return false;
    }

    if (!DoCheck)
        return true;

    // pieces
    for (PieceSet ps = v->pieceTypes; ps;)
    {
        PieceType pt = pop_lsb(ps);
        for (Color c : {WHITE, BLACK})
            if (std::count(v->pieceToChar.begin(), v->pieceToChar.end(), v->pieceToChar[make_piece(c, pt)]) != 1)
                std::cerr << piece_name(pt) << " - Ambiguous piece character: " << v->pieceToChar[make_piece(c, pt)] << std::endl;
    }

    v->conclude(); // In preparation for the consistency checks below

    // startFen
    if (FEN::validate_fen(v->startFen, v, v->chess960) != FEN::FEN_OK)
        std::cerr << "startFen - Invalid starting position: " << v->startFen << std::endl;

    // pieceToCharTable
    if (v->pieceToCharTable != "-")
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
    if (!v->checking && v->checkCounting)
        std::cerr << "checkCounting=true requires checking=true." << std::endl;
    if (v->progressiveMultimove && !v->multimoves.empty())
        std::cerr << "progressiveMultimove ignores multimoves sequence." << std::endl;
    for (Color c : {WHITE, BLACK})
        for (unsigned char ch : v->connectPieceGoal[c])
            if (!std::isspace(ch))
            {
                size_t idx = v->pieceToChar.find(std::toupper(ch));
                if (idx == std::string::npos || idx >= PIECE_TYPE_NB)
                    std::cerr << "connectPieceGoal" << (c == WHITE ? "White" : "Black")
                                  << " - Invalid piece type: " << char(ch) << std::endl;
            }
    if (v->castling && v->castlingRank > v->maxRank)
        std::cerr << "Inconsistent settings: castlingRank > maxRank." << std::endl;
    if (v->castling && v->castlingQueensideFile > v->castlingKingsideFile)
        std::cerr << "Inconsistent settings: castlingQueensideFile > castlingKingsideFile." << std::endl;
    if (v->connect3D && v->connect4D)
        std::cerr << "connect3D and connect4D are mutually exclusive." << std::endl;
    if (v->connect3D && !((int(v->maxFile) + 1) == 3 && (int(v->maxRank) + 1) == 9))
        std::cerr << "connect3D currently requires a 3x9 board." << std::endl;
    if (v->connect4D && !((int(v->maxFile) + 1) == 9 && (int(v->maxRank) + 1) == 9))
        std::cerr << "connect4D currently requires a 9x9 board." << std::endl;
    if ((v->connect3D || v->connect4D) && v->connectN != 3)
        std::cerr << "connect3D/connect4D currently require connectN = 3." << std::endl;

    // Options incompatible with royal kings
    if (v->pieceTypes & KING)
    {
        if (v->blastOnCapture)
            std::cerr << "Can not use kings with blastOnCapture." << std::endl;
        if (v->flipEnclosedPieces)
            std::cerr << "Can not use kings with flipEnclosedPieces." << std::endl;
        if (v->removeConnectN)
            std::cerr << "Can not use kings with removeConnectN." << std::endl;
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
                std::cerr << piece_name(v->kingType) << " is not supported as kingType." << std::endl;
        }
    }
    // Options incompatible with royal kings OR pseudo-royal kings. Possible in theory though:
    // 1. In blast variants, moving a (pseudo-)royal blastImmuneType into another piece is legal.
    // 2. In blast variants, capturing a piece next to a (pseudo-)royal blastImmuneType is legal.
    // 3. Moving a (pseudo-)royal mutuallyImmuneType into a square threatened by the same type is legal.
    if (v->pseudoRoyalTypes || v->antiRoyalTypes || (v->pieceTypes & KING))
    {
        if (v->blastImmuneTypes) //I may have this solved now.
            std::cerr << "Can not use kings, pseudo-royal, or anti-royal with blastImmuneTypes." << std::endl;
        if (v->mutuallyImmuneTypes)
            std::cerr << "Can not use kings, pseudo-royal, or anti-royal with mutuallyImmuneTypes." << std::endl;
    }
    if (v->flagPieceSafe && v->blastOnCapture)
    {
        std::cerr << "Can not use flagPieceSafe with blastOnCapture (flagPieceSafe uses simple assessment that does not see blast)." << std::endl;
        return false;
    }

    return true;
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
    auto parse_rank_value = [](const std::string& value, int& out) {
        return parse_positive_int(value, out);
    };
    auto parse_file_value = [](const std::string& value, int& out) {
        std::stringstream ss(value);
        ss >> std::ws;
        if (ss.peek() == EOF)
            return false;
        if (std::isdigit(ss.peek()))
        {
            int i;
            ss >> i;
            if (ss.fail())
                return false;
            out = i - 1;
            return i >= 1 && only_trailing_space(ss);
        }
        char c;
        ss >> c;
        if (ss.fail())
            return false;
        out = std::tolower(static_cast<unsigned char>(c)) - 'a';
        return only_trailing_space(ss);
    };

    int cfgMaxRank = -1;
    int cfgMaxFile = -1;
    const auto itRank = config.find("maxRank");
    if (itRank != config.end())
        parse_rank_value(itRank->second, cfgMaxRank);
    const auto itFile = config.find("maxFile");
    if (itFile != config.end())
        parse_file_value(itFile->second, cfgMaxFile);

    // Fail early when a variant exceeds compile-time board dimensions.
    if ((cfgMaxRank > 0 && cfgMaxRank - 1 > RANK_MAX) || (cfgMaxFile >= 0 && cfgMaxFile > FILE_MAX))
        return nullptr;

    parse_attribute("maxRank", v->maxRank);
    parse_attribute("maxFile", v->maxFile);

    if (!parse_piece_types(v) ||
        !parse_piece_values(v) ||
        !parse_legacy_attributes(v) ||
        !parse_official_options(v))
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
