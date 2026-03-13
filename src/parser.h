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

#include <iostream>

#include "variant.h"

namespace Stockfish {

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
        consumedKeys.insert(s);
        return data.find(s);
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
    template <bool Current = true, class T> bool parse_attribute(const std::string& key, T& target);
    template <bool Current = true, class T> bool parse_attribute(const std::string& key, T& target, const std::string& pieceToChar);

    bool parse_piece_types(Variant* v);
    bool parse_piece_values(Variant* v);
    bool parse_legacy_attributes(Variant* v);
    bool parse_official_options(Variant* v);
    void check_consistency(Variant* v);

    template <typename T> bool require_attribute(bool enabled, const std::string& key, T& target);
    template <typename T> void parse_both_colors(const std::string& key, T& target);
    template <typename T> void parse_both_colors_piece(const std::string& key, T& target, const std::string& pieceToChar);
};

} // namespace Stockfish

#endif // #ifndef PARSER_H_INCLUDED
