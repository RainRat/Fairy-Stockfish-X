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

#include <algorithm>
#include <cassert>
#include <cctype>
#include <charconv>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "types.h"
#include "parser.h"
#include "piece.h"

namespace Stockfish {

PieceMap pieceMap; // Global object


namespace {

  // Keep legacy/variant-facing aliases here:
  // L/C both mean camel (3,1), and J/Z both mean zebra (3,2).
  // In particular, built-in Janggi elephant notation still uses nZ.
  const std::map<char, std::vector<std::pair<int, int>>> leaperAtoms = {
      {'W', {std::make_pair(1, 0)}},
      {'F', {std::make_pair(1, 1)}},
      {'D', {std::make_pair(2, 0)}},
      {'N', {std::make_pair(2, 1)}},
      {'A', {std::make_pair(2, 2)}},
      {'H', {std::make_pair(3, 0)}},
      {'L', {std::make_pair(3, 1)}},
      {'C', {std::make_pair(3, 1)}},
      {'J', {std::make_pair(3, 2)}},
      {'Z', {std::make_pair(3, 2)}},
      {'G', {std::make_pair(3, 3)}},
      {'K', {std::make_pair(1, 0), std::make_pair(1, 1)}},
  };
  const std::map<char, std::vector<std::pair<int, int>>> riderAtoms = {
      {'R', {std::make_pair(1, 0)}},
      {'B', {std::make_pair(1, 1)}},
      {'Q', {std::make_pair(1, 0), std::make_pair(1, 1)}},
  };

  const std::string verticals = "fbvh";
  const std::string horizontals = "rlsh";

  std::string_view trim_view(std::string_view text) {
      const size_t first = text.find_first_not_of(" \t\r\n");
      if (first == std::string_view::npos)
          return std::string_view{};
      const size_t last = text.find_last_not_of(" \t\r\n");
      return text.substr(first, last - first + 1);
  }

  bool parse_piece_set(const Variant* variant, std::string_view text, PieceSet& target, bool allowAll = true, bool allowNone = true) {
      std::string_view remaining = trim_view(text);
      if (remaining.empty())
          return false;
      if (allowAll && remaining == "*")
      {
          if (!variant)
              return false;
          target = variant->pieceTypes;
          return true;
      }
      if (allowNone && remaining == "-")
      {
          target = NO_PIECE_SET;
          return true;
      }

      PieceSet parsed = NO_PIECE_SET;
      while (!remaining.empty())
      {
          while (!remaining.empty() && (remaining.front() == ',' || std::isspace(static_cast<unsigned char>(remaining.front()))))
              remaining.remove_prefix(1);
          if (remaining.empty())
              break;
          if (!Variant::is_piece_id_start(remaining.front()))
              return false;

          std::string token(1, remaining.front());
          if (remaining.size() >= 2 && Variant::is_piece_id_suffix(remaining[1]))
              token.push_back(remaining[1]);

          PieceType pt = variant ? variant->piece_type_from_symbol(token) : NO_PIECE_TYPE;
          if (pt == NO_PIECE_TYPE)
              return false;
          parsed |= piece_set(pt);
          remaining.remove_prefix(token.size());
      }
      target = parsed;
      return true;
  }

  void parse_min_max(std::string_view s, int& min_val, int& max_val, bool& fail_piece_flag) {
      size_t comma = s.find(',');
      if (comma != std::string_view::npos) {
          std::string_view min_s = trim_view(s.substr(0, comma));
          std::string_view max_s = trim_view(s.substr(comma + 1));

          auto safe_stoi = [&](std::string_view str, int default_val, bool& ok) {
              if (str.empty()) { ok = false; return default_val; }
              long long res = 0;
              ok = true;
              for (char ch : str) {
                  if (!std::isdigit(static_cast<unsigned char>(ch))) {
                      ok = false;
                      return default_val;
                  }
                  res = res * 10 + (ch - '0');
                  if (res > std::numeric_limits<int>::max()) {
                      ok = false;
                      return default_val;
                  }
              }
              return static_cast<int>(res);
          };

          bool minOk = false, maxOk = false;
          min_val = safe_stoi(min_s, 1, minOk);
          if (max_s == "*")
          {
              max_val = 255;
              maxOk = true;
          }
          else
              max_val = safe_stoi(max_s, 1, maxOk);
          if (!minOk || (!maxOk && max_s != "*"))
          {
              std::cerr << "Invalid numeric value in Betza hopper parameters: '" << s << "'" << std::endl;
              fail_piece_flag = true;
              return;
          }
          if (minOk && (maxOk || max_s == "*") && min_val > max_val)
          {
              std::cerr << "Invalid hopper range (min > max) in Betza hopper parameters: '" << s << "'" << std::endl;
              fail_piece_flag = true;
              return;
          }
          return;
      }
      else
      {
          std::cerr << "Invalid hopper range (missing comma) in Betza hopper parameters: '" << s << "'" << std::endl;
          fail_piece_flag = true;
          return;
      }
  }

  void parse_hopper_or_lame_block(
      std::string_view params,
      const std::string& betza,
      const Variant* variant,
      bool lame,
      bool& hasLameProfile,
      bool& invalidLameProfile,
      PieceInfo::LameProfile& currentLameProfile,
      bool& hasUniversalHopper,
      PieceInfo::HopperProfile& currentHopperProfile,
      bool& invalidPiece
  ) {
      if (lame)
      {
          if (hasLameProfile)
              invalidLameProfile = true;
          hasLameProfile = true;
          currentLameProfile = {};
      }
      else
      {
          hasUniversalHopper = true;
          currentHopperProfile = {};
      }

      size_t pos = 0;
      const bool blockIsLame = lame;
      while (pos < params.size()) {
          size_t next_semi = params.find(';', pos);
          if (next_semi == std::string_view::npos) next_semi = params.size();
          std::string_view pair = trim_view(params.substr(pos, next_semi - pos));
          size_t colon = pair.find(':');
          if (colon != std::string_view::npos) {
              std::string_view key = trim_view(pair.substr(0, colon));
              std::string_view val = trim_view(pair.substr(colon + 1));

              if (blockIsLame)
              {
                  if (key == "path") {
                      if (val == "default" || val == "mao" || val == "orthfirst")
                          currentLameProfile.path = PieceInfo::LameProfile::ORTH_FIRST;
                      else if (val == "moa" || val == "diagfirst")
                          currentLameProfile.path = PieceInfo::LameProfile::DIAG_FIRST;
                      else if (val == "anypath" || val == "either" || val == "both")
                          currentLameProfile.path = PieceInfo::LameProfile::ANY_PATH;
                      else if (val == "mid")
                          currentLameProfile.path = PieceInfo::LameProfile::MIDPOINT;
                      else
                      {
                          std::cerr << "Unknown Betza lame path '" << val << "' in '" << betza << "'." << std::endl;
                          invalidLameProfile = true;
                      }
                  }
                  else
                  {
                      std::cerr << "Unknown Betza parameter key '" << key << "' in lame block of '" << betza << "'." << std::endl;
                      invalidLameProfile = true;
                  }
              }
              else
              {
                  if (key == "hurdles") { parse_min_max(val, currentHopperProfile.hurdlesMin, currentHopperProfile.hurdlesMax, invalidPiece); }
                  else if (key == "pre") { parse_min_max(val, currentHopperProfile.preMin, currentHopperProfile.preMax, invalidPiece); }
                  else if (key == "post") { parse_min_max(val, currentHopperProfile.postMin, currentHopperProfile.postMax, invalidPiece); }
                  else if (key == "capture") {
                      if (val == "dest") currentHopperProfile.captureMode = PieceInfo::CAPTURE_DEST;
                      else if (val == "locust_all") currentHopperProfile.captureMode = PieceInfo::CAPTURE_LOCUST_ALL;
                      else if (val == "locust_first") currentHopperProfile.captureMode = PieceInfo::CAPTURE_LOCUST_FIRST;
                      else if (val == "locust_last") currentHopperProfile.captureMode = PieceInfo::CAPTURE_LOCUST_LAST;
                      else {
                          std::cerr << "Unknown Betza hopper capture mode '" << val << "' in '" << betza << "'." << std::endl;
                          invalidPiece = true;
                      }
                  }
                  else if (key == "equi") {
                      if (val == "hopper") currentHopperProfile.equiRule = PieceInfo::EQUI_HOPPER;
                      else if (val == "stopper") currentHopperProfile.equiRule = PieceInfo::EQUI_STOPPER;
                      else
                      {
                          std::cerr << "Unknown Betza hopper equi mode '" << val << "' in '" << betza << "'." << std::endl;
                          invalidPiece = true;
                      }
                  }
                  else if (key == "hurdle_types" || key == "transparent_types") {
                      bool isHurdle = (key == "hurdle_types");
                      uint8_t& special = isHurdle ? currentHopperProfile.hurdleSpecialTypes : currentHopperProfile.transparentSpecialTypes;
                      special = PieceInfo::HopperProfile::NONE; // Reset default for explicit types

                      size_t vpos = 0;
                      while (vpos < val.size()) {
                          size_t next_comma = val.find(',', vpos);
                          if (next_comma == std::string_view::npos) next_comma = val.size();
                          std::string_view typeToken = trim_view(val.substr(vpos, next_comma - vpos));

                          if (typeToken == "enemy") special |= PieceInfo::HopperProfile::ENEMY;
                          else if (typeToken == "friendly") special |= PieceInfo::HopperProfile::FRIENDLY;
                          else if (typeToken == "wall") special |= PieceInfo::HopperProfile::WALL;
                          else if (typeToken == "dead") special |= PieceInfo::HopperProfile::DEAD;
                          else if (!typeToken.empty())
                          {
                              std::cerr << "Unknown Betza hopper special type '" << typeToken << "' in '" << betza << "'." << std::endl;
                              invalidPiece = true;
                          }

                          vpos = next_comma + 1;
                      }
                  }
                  else if (key == "hurdle_piece_types" || key == "transparent_piece_types") {
                      bool isHurdle = (key == "hurdle_piece_types");
                      PieceSet& target = isHurdle ? currentHopperProfile.hurdlePieceTypes : currentHopperProfile.transparentPieceTypes;
                      if (!parse_piece_set(variant, val, target, true, true))
                      {
                          std::cerr << "Unknown Betza hopper piece type list '" << val << "' in '" << betza << "'." << std::endl;
                          invalidPiece = true;
                      }
                  }
                  else
                  {
                      std::cerr << "Unknown Betza parameter key '" << key << "' in hopper block of '" << betza << "'." << std::endl;
                      invalidPiece = true;
                  }
              }
          }
          if (invalidPiece)
              break;
          pos = next_semi + 1;
      }
      if (blockIsLame && invalidLameProfile)
      {
          invalidPiece = true;
      }
  }

  // from_betza creates a piece by parsing Betza notation
  // https://en.wikipedia.org/wiki/Betza%27s_funny_notation
  PieceInfo* from_betza(const std::string& betza, const std::string& name, const Variant* variant = nullptr) {
      std::unique_ptr<PieceInfo> p = std::make_unique<PieceInfo>();
      p->name = name;
      p->betza = betza;

      // Convenience aliases for common fairy pieces in customPiece Betza fields.
      auto alias_to_betza = [](const std::string& in) {
          std::string key;
          key.reserve(in.size());
          for (char ch : in)
          {
              if (std::isalnum(static_cast<unsigned char>(ch)))
                  key.push_back(std::tolower(static_cast<unsigned char>(ch)));
          }
          static const std::map<std::string, std::string> aliasMap = {
              {"wazir", "W"},
              {"fers", "F"},
              {"ferz", "F"},
              {"alfil", "A"},
              {"dabbaba", "D"},
              {"camel", "L"},
              {"zebra", "J"},
              {"nightrider", "NN"},
              {"grasshopper", "gQ"},
              {"rose", "@"},
              {"circularknight", "@"},
              {"mann", "K"},
              {"amazon", "QN"},
              {"chancellor", "RN"},
              {"archbishop", "BN"},
              {"marshall", "RN"},
              {"empress", "RN"},
              {"cardinal", "BN"},
              {"princess", "BN"}
          };
          auto it = aliasMap.find(key);
          return it == aliasMap.end() ? in : it->second;
      };

      // Parser sugar: m(AB) -> mAmB, c(RB) -> cRcB
      auto expand_group_sugar = [&](const std::string& in) {
          const std::string prefixChars = "mcpgnjzxifbrlvsh";
          std::string out;
          for (std::string::size_type i = 0; i < in.size(); ++i)
          {
              if (in[i] != '(')
              {
                  out.push_back(in[i]);
                  continue;
              }
              auto close = in.find(')', i + 1);
              if (close == std::string::npos)
              {
                  out.push_back(in[i]);
                  continue;
              }
              std::string content = in.substr(i + 1, close - i - 1);
              if (content.empty() || content.find(',') != std::string::npos)
              {
                  out.append(in, i, close - i + 1);
                  i = close;
                  continue;
              }
              bool groupAtomsOnly = true;
              for (char gc : content)
                  if (leaperAtoms.find(gc) == leaperAtoms.end() && riderAtoms.find(gc) == riderAtoms.end() && gc != 'U' && gc != 'O' && gc != 'M')
                  {
                      groupAtomsOnly = false;
                      break;
                  }
              if (!groupAtomsOnly)
              {
                  out.append(in, i, close - i + 1);
                  i = close;
                  continue;
              }

              std::string prefix;
              while (!out.empty() && prefixChars.find(out.back()) != std::string::npos)
              {
                  prefix.push_back(out.back());
                  out.pop_back();
              }
              std::reverse(prefix.begin(), prefix.end());
              if (prefix.empty())
              {
                  out.append(in, i, close - i + 1);
                  i = close;
                  continue;
              }
              for (char gc : content)
              {
                  out += prefix;
                  out.push_back(gc);
              }
              i = close;
          }
          return out;
      };

      const std::string expandedBetza = expand_group_sugar(alias_to_betza(betza));
      std::vector<MoveModality> moveModalities = {};
      bool hopper = false;
      bool rider = false;
      bool lame = false;
      bool hasLameProfile = false;
      bool invalidLameProfile = false;
      bool invalidPiece = false;
      PieceInfo::LameProfile currentLameProfile;
      bool initial = false;
      bool dynamicDistance = false;
      bool skiSlider = false;
      bool maxDistance = false;
      int distance = 0;
      bool standaloneH = false;
      std::vector<std::string> prelimDirections = {};
      bool hasUniversalHopper = false;
      PieceInfo::HopperProfile currentHopperProfile;

      auto reset_parser_state = [&]() {
          moveModalities.clear();
          prelimDirections.clear();
          hopper = false;
          rider = false;
          lame = false;
          hasLameProfile = false;
          invalidLameProfile = false;
          currentLameProfile = {};
          initial = false;
          dynamicDistance = false;
          skiSlider = false;
          maxDistance = false;
          standaloneH = false;
          distance = 0;
          hasUniversalHopper = false;
          currentHopperProfile = {};
      };
      auto fail_piece = [&]() {
          invalidPiece = true;
      };
      auto ensure_default_modalities = [&]() {
          if (moveModalities.empty())
          {
              moveModalities.push_back(MODALITY_QUIET);
              moveModalities.push_back(MODALITY_CAPTURE);
          }
      };

      auto commit_atom = [&](const std::vector<std::pair<int, int>>& atoms, bool atomIsRider, std::string::size_type& i, char atomChar, bool atomIsTuple = false) {
          // Check for rider / limited-distance rider suffix.
          rider = atomIsRider;
          if (i + 1 < expandedBetza.size())
          {
              if (expandedBetza[i + 1] == atomChar)
              {
                  rider = true;
                  i++;
              }
              else if (std::isdigit(static_cast<unsigned char>(expandedBetza[i + 1])))
              {
                  rider = true;
                  int parsedDistance = 0;
                  std::string::size_type j = i + 1;
                  while (j < expandedBetza.size() && std::isdigit(static_cast<unsigned char>(expandedBetza[j])))
                  {
                      parsedDistance = std::min(parsedDistance * 10 + (expandedBetza[j] - '0'), 255);
                      j++;
                  }
                  if (parsedDistance == 0)
                  {
                      std::cerr << "Invalid Betza rider range in '" << betza << "': distance must be greater than zero." << std::endl;
                      fail_piece();
                      return;
                  }
                  distance = parsedDistance;
                  i = j - 1;
              }
              else if (expandedBetza[i + 1] == '[')
              {
                  auto report_invalid_range = [&]() {
                      std::cerr << "Invalid Betza rider range in '" << betza
                                << "': use [n-m] or [n-], and keep existing Rn syntax for max-only ranges." << std::endl;
                      fail_piece();
                  };
                  auto close = expandedBetza.find(']', i + 2);
                  if (close == std::string::npos)
                  {
                      std::cerr << "Invalid Betza rider range in '" << betza
                                << "': missing closing ']'." << std::endl;
                      fail_piece();
                      return;
                  }
                  std::string rangeSpec = expandedBetza.substr(i + 2, close - i - 2);
                  std::size_t dash = rangeSpec.find('-');
                  bool unsupportedCombo = !atomIsRider || atomIsTuple || hopper || lame || dynamicDistance || skiSlider || maxDistance;
                  bool malformedRange = dash == std::string::npos                                     || rangeSpec.find('-', dash + 1) != std::string::npos
                                     || dash == 0;
                  if (unsupportedCombo)
                  {
                      std::cerr << "Unsupported Betza rider range in '" << betza
                                << "': bracketed ranges currently support plain rider atoms such as R[3-5] or R[3-]." << std::endl;
                      fail_piece();
                      return;
                  }
                  if (malformedRange)
                  {
                      report_invalid_range();
                      return;
                  }
                  int minDistance = 0;
                  int parsedMaxDistance = 0;
                  std::string minPart = rangeSpec.substr(0, dash);
                  std::string maxPart = rangeSpec.substr(dash + 1);
                  if (!parse_positive_int(minPart, minDistance)
                      || minDistance <= 0
                      || (!maxPart.empty() && (!parse_positive_int(maxPart, parsedMaxDistance) || parsedMaxDistance < minDistance))
                      || (maxPart.empty() && rangeSpec.back() != '-'))
                  {
                      report_invalid_range();
                      return;
                  }
                  if (maxPart.empty())
                      parsedMaxDistance = 0;
                  rider = true;
                  distance = encode_slider_range(minDistance, parsedMaxDistance);
                  i = close;
              }
          }
          if (!rider && lame)
              distance = -1;
          if (rider && skiSlider && !hopper && !lame)
              distance = SKI_SLIDER_LIMIT;
          if (rider && maxDistance && !hopper && !lame && !skiSlider)
          {
              distance = MAX_SLIDER_LIMIT;
              p->add_rider_augment(PieceInfo::AUGMENT_MAX);
          }
          if (dynamicDistance && rider)
          {
              distance = DYNAMIC_SLIDER_LIMIT;
              p->add_rider_augment(PieceInfo::AUGMENT_DYNAMIC);
          }

          if (initial && std::find(moveModalities.begin(), moveModalities.end(), MODALITY_CAPTURE) != moveModalities.end())
          {
              std::cerr << "Initial capture Betza moves are not supported in '" << betza
                        << "': remove the capture modality or the initial modifier." << std::endl;
              fail_piece();
              return;
          }
          if (lame && atomIsTuple)
          {
              std::cerr << "Unsupported Betza tuple modifier combination in '" << betza
                        << "': lame path profiles currently apply to named step/leaper and rider atoms only." << std::endl;
              fail_piece();
              return;
          }
          if (lame && (hopper || dynamicDistance || skiSlider || maxDistance || hasUniversalHopper))
          {
              std::cerr << "Unsupported Betza lame modifier combination in '" << betza
                        << "': lame path profiles currently apply to step/leaper and rider atoms only." << std::endl;
              fail_piece();
              return;
          }
          ensure_default_modalities();
          // Define moves for each atom and modality.
          for (const auto& atom : atoms)
          {
              std::vector<std::string> directions = {};
              // Split directions for orthogonal pieces (e.g. fsW for soldier).
              for (auto s : prelimDirections)
                  if (atoms.size() == 1 && atom.second == 0 && s[0] != s[1] && s != "hr" && s != "hl")
                  {
                      directions.push_back(std::string(2, s[0]));
                      directions.push_back(std::string(2, s[1]));
                  }
                  else
                      directions.push_back(s);

              // Add moves to steps/slider/hopper tables.
              for (auto modality : moveModalities)
              {
                  auto& leapRiderV = p->leapRider[initial][modality];
                  auto& tupleV = p->tupleSteps[initial][modality];
                  auto& tupleSliderV = p->tupleSlider[initial][modality];
                  auto has_dir = [&](std::string_view s) {
                    return std::find(directions.begin(), directions.end(), s) != directions.end();
                  };
                  auto add_step = [&](int dr, int df) {
                      if (hasUniversalHopper) {
                          p->universalHopper[initial][modality][Direction(dr * FILE_NB + df)] = currentHopperProfile;
                      } else {
                          if (atomIsTuple && !hopper && rider)
                              tupleSliderV.push_back({dr, df, distance});
                          else if (atomIsTuple && !hopper && !rider)
                              tupleV.emplace_back(dr, df);
                          else
                          {
                              if (lame)
                              {
                                  // Lame profiles use PieceInfo::LameProfile's limit convention:
                                  // -1 for a single leap, 0 for an unlimited rider, positive for a max hop count.
                                  currentLameProfile.limit = rider ? distance : -1;
                                  p->stepsLame[initial][modality][Direction(dr * FILE_NB + df)] = currentLameProfile;
                              }
                              else
                              {
                                  auto& v = hopper ? p->hopper[initial][modality]
                                           : rider ? p->slider[initial][modality]
                                                   : p->steps[initial][modality];
                                  v[Direction(dr * FILE_NB + df)] = distance;
                              }
                              if (rider && !atomIsRider && !hopper
                                  && !lame && !dynamicDistance && !skiSlider && !maxDistance)
                                  leapRiderV[Direction(dr * FILE_NB + df)] = distance;
                          }
                      }
                  };
                  struct DirRule {
                      bool swap;
                      int multR, multF;
                      std::initializer_list<const char*> codes;
                      bool hOnlyNonStandalone;
                      const char* hCode;
                  };
                  static const DirRule rules[] = {
                      {false,  1,  1, {"ff", "vv", "rf", "rv", "fh", "rh"}, true,  "hr"},
                      {false, -1, -1, {"bb", "vv", "lb", "lv", "bh", "lh"}, true,  "hr"},
                      {true,  -1,  1, {"rr", "ss", "br", "bs", "bh", "rh"}, false, "hr"},
                      {true,   1, -1, {"ll", "ss", "fl", "fs", "fh", "lh"}, false, "hr"},
                      {true,   1,  1, {"rr", "ss", "fr", "fs", "fh", "rh"}, false, "hl"},
                      {true,  -1, -1, {"ll", "ss", "bl", "bs", "bh", "lh"}, false, "hl"},
                      {false, -1,  1, {"bb", "vv", "rb", "rv", "bh", "rh"}, true,  "hl"},
                      {false,  1, -1, {"ff", "vv", "lf", "lv", "fh", "lh"}, true,  "hl"}
                  };
                  for (const auto& rule : rules)
                  {
                      bool match = directions.empty();
                      if (!match)
                      {
                          for (const char* code : rule.codes)
                              if (has_dir(code))
                              {
                                  match = true;
                                  break;
                              }
                      }
                      if (!match)
                      {
                          if (has_dir(rule.hCode))
                              match = !rule.hOnlyNonStandalone || !standaloneH;
                      }
                      if (match)
                      {
                          int dr = rule.multR * (rule.swap ? atom.second : atom.first);
                          int df = rule.multF * (rule.swap ? atom.first : atom.second);
                          add_step(dr, df);
                      }
                  }
              }
          }
          // Reset per-atom parser state.
          reset_parser_state();
      };

      auto commit_bent_slider = [&](auto flag, const char* pieceName) {
          // Keep first implementation strict: unqualified O only.
          if (!prelimDirections.empty() || hopper || lame || dynamicDistance || skiSlider || maxDistance)
          {
              std::cerr << "Modifiers are not yet implemented for " << pieceName << " in '" << betza << "'." << std::endl;
              fail_piece();
              return;
          }
          ensure_default_modalities();
          for (auto modality : moveModalities)
              ((*p).*flag)[initial][modality] = true;
          reset_parser_state();
      };

      for (std::string::size_type i = 0; i < expandedBetza.size(); i++)
      {
          if (invalidPiece)
              break;

          char c = expandedBetza[i];
          // Universal Hopper config
          if (c == '{')
          {
              auto close = expandedBetza.find('}', i + 1);
              if (close == std::string::npos)
              {
                  std::cerr << "Invalid Betza hopper parameters in '" << betza << "': missing closing '}'." << std::endl;
                  fail_piece();
                  continue;
              }
              std::string_view params(expandedBetza.data() + i + 1, close - i - 1);
              parse_hopper_or_lame_block(params, betza, variant, lame, hasLameProfile, invalidLameProfile, currentLameProfile, hasUniversalHopper, currentHopperProfile, invalidPiece);
              i = close;
          }
          // Modality
          else if (c == 'm' || c == 'c')
              moveModalities.push_back(c == 'c' ? MODALITY_CAPTURE : MODALITY_QUIET);
          // Hopper (grasshopper when g)
          else if (c == 'p' || c == 'g')
          {
              hopper = true;
              if (c == 'g')
                  distance = 1;
          }
          // Lame leaper
          else if (c == 'n')
              lame = true;
          // Dynamic distance slider
          else if (c == 'x')
              dynamicDistance = true;
          // Ski/slip slider modifier (e.g. jR, jB, jQ)
          else if (c == 'j')
              skiSlider = true;
          // Max-distance slider modifier (e.g. zR, zB, zQ)
          else if (c == 'z')
              maxDistance = true;
          // Initial move
          else if (c == 'i')
              initial = true;
          // Rifle-capture syntax marker for per-piece shot captures.
          else if (c == '^')
              p->rifleCapture = true;
          // Directional modifiers
          else if (verticals.find(c) != std::string::npos || horizontals.find(c) != std::string::npos)
          {
              if (i + 1 < expandedBetza.size())
              {
                  char c2 = expandedBetza[i + 1];
                  if (   c2 == c
                      || (verticals.find(c) != std::string::npos && horizontals.find(c2) != std::string::npos)
                      || (horizontals.find(c) != std::string::npos && verticals.find(c2) != std::string::npos))
                  {
                      std::string combo = std::string(1, c) + c2;
                      if ((c == 'h' || c2 == 'h') && combo != "hr" && combo != "hl" && combo != "fh" && combo != "bh" && combo != "rh" && combo != "lh")
                      {
                          std::cerr << "Invalid Betza direction modifier combination: '" << combo << "' in '" << betza << "'." << std::endl;
                          fail_piece();
                          continue;
                      }
                      prelimDirections.push_back(combo);
                      i++;
                      continue;
                  }
              }
              if (c == 'h')
              {
                  prelimDirections.push_back("hr");
                  prelimDirections.push_back("hl");
                  standaloneH = true;
              }
              else
                  prelimDirections.push_back(std::string(2, c));
          }
          // Standard Betza move atom
          else if (auto leaperIt = leaperAtoms.find(c); leaperIt != leaperAtoms.end())
          {
              commit_atom(leaperIt->second, false, i, c);
          }
          else if (auto riderIt = riderAtoms.find(c); riderIt != riderAtoms.end())
          {
              commit_atom(riderIt->second, true, i, c);
          }
          // Universal leaper: U can target any square on board.
          else if (c == 'U')
          {
              std::vector<std::pair<int, int>> universalAtoms;
              universalAtoms.reserve((int(RANK_MAX) + 1) * (int(FILE_MAX) + 1) - 1);
              for (int dr = 0; dr <= int(RANK_MAX); ++dr)
                  for (int df = 0; df <= int(FILE_MAX); ++df)
                      if (dr != 0 || df != 0)
                          universalAtoms.emplace_back(dr, df);
              commit_atom(universalAtoms, false, i, c);
          }
          // Griffon bent slider (one diagonal step, then outward rook slide)
          else if (c == 'O')
              commit_bent_slider(&PieceInfo::griffon, "bent slider");
          // Manticore bent slider (one orthogonal step, then outward bishop slide)
          else if (c == 'M')
              commit_bent_slider(&PieceInfo::manticore, "bent slider");
          // Standard rose/circular knight rider.
          else if (c == '@')
              commit_bent_slider(&PieceInfo::rose, "rose");
          // Tuple atom: (x,y), optionally repeated or numeric for riders.
          else if (c == '(')
          {
              if (hopper || lame || dynamicDistance || skiSlider || maxDistance)
              {
                  std::cerr << "Unsupported Betza tuple modifier combination in '" << betza
                            << "': tuple atoms only support explicit leapers or repeated/numeric tuple riders." << std::endl;
                  fail_piece();
                  auto closeUnsupported = expandedBetza.find(')', i + 1);
                  if (closeUnsupported != std::string::npos)
                      i = closeUnsupported;
                  continue;
              }
              auto close = expandedBetza.find(')', i + 1);
              if (close == std::string::npos)
              {
                  fail_piece();
                  continue;
              }
              auto comma = expandedBetza.find(',', i + 1);
              if (comma == std::string::npos || comma > close)
              {
                  fail_piece();
                  i = close;
                  continue;
              }
              int dx = 0, dy = 0;
              if (!parse_int_strict(expandedBetza.substr(i + 1, comma - i - 1), dx) || dx < 0
                  || !parse_int_strict(expandedBetza.substr(comma + 1, close - comma - 1), dy) || dy < 0)
              {
                  fail_piece();
                  i = close;
                  continue;
              }
              // Tuple atoms are stored as (rankDelta, fileDelta).
              if ((dx == 0 && dy == 0) || dx > int(RANK_MAX) || dy > int(FILE_MAX))
              {
                  fail_piece();
                  i = close;
                  continue;
              }
              std::vector<std::pair<int, int>> tupleAtom = { std::make_pair(dx, dy) };
              std::string tupleText = expandedBetza.substr(i, close - i + 1);
              std::string::size_type next = close + 1;
              bool repeatedTupleRider = next < expandedBetza.size()
                                     && expandedBetza.compare(next, tupleText.size(), tupleText) == 0;
              i = repeatedTupleRider ? next + tupleText.size() - 1 : close;
              commit_atom(tupleAtom, repeatedTupleRider, i, ')', true);
          }
      }
      if (invalidPiece)
          return nullptr;
      return p.release();
  }

  // Special multi-leg betza description for Janggi elephant
  PieceInfo* janggi_elephant_piece() {
      PieceInfo* p = from_betza("nZ", "janggiElephant");
      assert(p);
      p->betza = "mafsmafW"; // for compatibility with XBoard/Winboard
      return p;
  }
}

bool validate_custom_piece_betza(const std::string& betza, const std::string& name, const Variant* variant) {
    std::unique_ptr<PieceInfo> p(from_betza(betza, name, variant));
    return bool(p);
}

void PieceMap::init(const Variant* v) {
  clear_all();
  add(PAWN, from_betza("fmWfceF", "pawn"));
  add(KNIGHT, from_betza("N", "knight"));
  add(BISHOP, from_betza("B", "bishop"));
  add(ROOK, from_betza("R", "rook"));
  add(QUEEN, from_betza("Q", "queen"));
  add(FERS, from_betza("F", "fers"));
  add(ALFIL, from_betza("A", "alfil"));
  add(FERS_ALFIL, from_betza("FA", "fersAlfil"));
  add(SILVER, from_betza("FfW", "silver"));
  add(AIWOK, from_betza("RNF", "aiwok"));
  add(BERS, from_betza("RF", "bers"));
  add(ARCHBISHOP, from_betza("BN", "archbishop"));
  add(CHANCELLOR, from_betza("RN", "chancellor"));
  add(AMAZON, from_betza("QN", "amazon"));
  add(KNIBIS, from_betza("mNcB", "knibis"));
  add(BISKNI, from_betza("mBcN", "biskni"));
  add(KNIROO, from_betza("mNcR", "kniroo"));
  add(ROOKNI, from_betza("mRcN", "rookni"));
  add(SHOGI_PAWN, from_betza("fW", "shogiPawn"));
  add(LANCE, from_betza("fR", "lance"));
  add(SHOGI_KNIGHT, from_betza("fN", "shogiKnight"));
  add(GOLD, from_betza("WfF", "gold"));
  add(DRAGON_HORSE, from_betza("BW", "dragonHorse"));
  add(CLOBBER_PIECE, from_betza("cW", "clobber"));
  add(BREAKTHROUGH_PIECE, from_betza("fmWfF", "breakthrough"));
  add(IMMOBILE_PIECE, from_betza("", "immobile"));
  add(CANNON, from_betza("mRcpR", "cannon"));
  add(JANGGI_CANNON, from_betza("pR", "janggiCannon"));
  add(SOLDIER, from_betza("fsW", "soldier"));
  add(HORSE, from_betza("nN", "horse"));
  add(ELEPHANT, from_betza("nA", "elephant"));
      add(JANGGI_ELEPHANT, janggi_elephant_piece());
      add(BANNER, from_betza("RcpRnN", "banner"));
      add(WAZIR, from_betza("W", "wazir"));
      add(COMMONER, from_betza("K", "commoner"));
      add(CENTAUR, from_betza("KN", "centaur"));
      add(KING, from_betza("K", "king"));
      // Add custom pieces
      for (PieceType pt = CUSTOM_PIECES; pt <= CUSTOM_PIECES_END; ++pt)
      add(pt, from_betza(v != nullptr ? v->customPiece[pt - CUSTOM_PIECES] : "", "", v));
}

void PieceMap::add(PieceType pt, PieceInfo* p) {
  if (p)
  {
      auto is_diagonal_only_slider = [](const std::map<Direction, int>& sliderMap) {
          if (sliderMap.empty())
              return false;
          for (auto const& [dir, _] : sliderMap)
              if (dir != NORTH_EAST && dir != NORTH_WEST && dir != SOUTH_EAST && dir != SOUTH_WEST)
                  return false;
          return true;
      };

      bool diagonalOnly = is_diagonal_only_slider(p->slider[0][MODALITY_QUIET])
                       || is_diagonal_only_slider(p->slider[0][MODALITY_CAPTURE]);

      // The identical step terms (p->steps * 100) are added to both numerator and denominator
      // as a weighted average. This ensures that hybrid pieces (slider + steps) only scale down
      // the mobility contribution of their sliding moves, without incorrectly scaling down
      // the mobility contribution of their step moves (which have no range limitation).
      int currentFrac = Stockfish::slider_fraction(p->slider[0][MODALITY_QUIET]) + Stockfish::slider_fraction(p->slider[0][MODALITY_CAPTURE])
                      + (p->steps[0][MODALITY_QUIET].size() + p->steps[0][MODALITY_CAPTURE].size()) * 100;
      int standardFrac = (p->slider[0][MODALITY_QUIET].size() + p->slider[0][MODALITY_CAPTURE].size()) * 100
                       + (p->steps[0][MODALITY_QUIET].size() + p->steps[0][MODALITY_CAPTURE].size()) * 100;

      if (diagonalOnly && standardFrac > 0 && currentFrac < standardFrac)
      {
          p->mobilityScaling = std::max(1, currentFrac * 100 / standardFrac);
          p->diagonalLimitedSlider = true;
      }
  }

  auto it = find(pt);
  if (it != end() && it->second != p) {
      delete it->second;
  }
  (*this)[pt] = p;
  direct[pt] = p;
  if (p && p->has_runtime_rider_augment())
      runtimeRiderAugmentTypes |= piece_set(pt);
  else
      runtimeRiderAugmentTypes &= ~piece_set(pt);
}

void PieceMap::clear_all() {
  for (auto const& element : *this)
      delete element.second;
  clear();
  direct.fill(nullptr);
  runtimeRiderAugmentTypes = PieceSet(0);
}

} // namespace Stockfish
