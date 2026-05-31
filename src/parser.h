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

#ifndef PARSER_H_INCLUDED
#define PARSER_H_INCLUDED

#include <charconv>
#include <cctype>
#include <iostream>
#include <string>

#include "variant.h"

namespace Stockfish {

inline bool parse_file_index(const std::string& raw, int& out) {
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

inline bool parse_positive_int(const std::string& raw, int& out) {
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

class Config {
public:
    using MapType = std::map<std::string, std::string>;
    using iterator = MapType::const_iterator;
    using const_iterator = MapType::const_iterator;

    Config() = default;

    std::string& operator[](const std::string& key) {
        return data[key];
    }

    size_t count(const std::string& key) const {
        return data.count(key);
    }

    const_iterator find (const std::string& s) const {
        constexpr bool PrintOptions = false; // print config options?
        if (PrintOptions)
            std::cout << s << std::endl;
        const auto it = data.find(s);
        if (it != data.end())
            consumedKeys.insert(s);
        return it;
    }

    const_iterator begin() const { return data.begin(); }
    const_iterator end() const { return data.end(); }

    const std::set<std::string>& get_consumed_keys() const {
        return consumedKeys;
    }
private:
    MapType data;
    mutable std::set<std::string> consumedKeys = {};
};

template <bool DoCheck>
class VariantParser {
public:
    VariantParser(const Config& c) : config (c) {};
    Variant* parse();
    Variant* parse(Variant* v);

private:
    Config config;
    bool parseHadError = false;
    template <bool Current = true, class T> bool parse_attribute(const std::string& key, T& target);
    template <bool Current = true, class T> bool parse_attribute(const std::string& key, T& target, const Variant* v);

    bool parse_piece_types(Variant* v);
    bool parse_piece_values(Variant* v);
    bool parse_legacy_attributes(Variant* v);
    bool parse_official_options(Variant* v);
    bool check_consistency(Variant* v);

    template <typename T> void apply_color_setting(ColorSetting<T>& target, Color color, const T& parsed);
    template <typename T> void parse_color_setting(const std::string& key, ColorSetting<T>& target);
    bool parse_color_setting_first_piece(const std::string& key, ColorSetting<PieceType>& target, const Variant* v);
    template <typename T> bool parse_color_setting_piece(const std::string& key, ColorSetting<T>& target, const Variant* v);
};

} // namespace Stockfish

#endif // #ifndef PARSER_H_INCLUDED
