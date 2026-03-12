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
#include <cctype>
#include <iostream>
#include <limits>
#include <map>
#include <string>
#include <utility>

#include "types.h"
#include "piece.h"

namespace Stockfish {

PieceMap pieceMap; // Global object


namespace {
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
  // from_betza creates a piece by parsing Betza notation
  // https://en.wikipedia.org/wiki/Betza%27s_funny_notation
  PieceInfo* from_betza(const std::string& betza, const std::string& name) {
      PieceInfo* p = new PieceInfo();
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
          const std::string prefixChars = "mcpgnojzxiyfbrlvsh";
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
                  prefix.insert(prefix.begin(), out.back());
                  out.pop_back();
              }
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

      auto parse_positive_int = [](const std::string& s, int& out) {
          if (s.empty())
              return false;
          long long v = 0;
          for (char ch : s)
          {
              if (!std::isdigit(static_cast<unsigned char>(ch)))
                  return false;
              v = v * 10 + (ch - '0');
              if (v > std::numeric_limits<int>::max())
                  return false;
          }
          out = int(v);
          return true;
      };

      const std::string expandedBetza = expand_group_sugar(alias_to_betza(betza));
      std::vector<MoveModality> moveModalities = {};
      bool hopper = false;
      bool contraHopper = false;
      bool rider = false;
      bool lame = false;
      bool initial = false;
      bool dynamicDistance = false;
      bool skiSlider = false;
      bool maxDistance = false;
      int distance = 0;
      bool standaloneH = false;
      std::vector<std::string> prelimDirections = {};

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
                  distance = parsedDistance;
                  i = j - 1;
              }
          }
          if (!rider && lame)
              distance = -1;
          if (rider && skiSlider && !hopper && !lame && !dynamicDistance)
              distance = SKI_SLIDER_LIMIT;
          if (rider && maxDistance && !hopper && !lame && !dynamicDistance && !skiSlider)
          {
              distance = MAX_SLIDER_LIMIT;
              p->add_rider_augment(PieceInfo::AUGMENT_MAX);
          }
          if (dynamicDistance && rider)
          {
              distance = DYNAMIC_SLIDER_LIMIT;
              p->add_rider_augment(PieceInfo::AUGMENT_DYNAMIC);
          }
          if (moveModalities.size() == 0)
          {
              moveModalities.push_back(MODALITY_QUIET);
              moveModalities.push_back(MODALITY_CAPTURE);
          }
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
                  auto& v = hopper ? p->hopper[initial][modality]
                           : contraHopper ? p->contraHopper[initial][modality]
                           : rider ? p->slider[initial][modality]
                                   : p->steps[initial][modality];
                  auto& tupleV = p->tupleSteps[initial][modality];
                  auto has_dir = [&](std::string s) {
                    return std::find(directions.begin(), directions.end(), s) != directions.end();
                  };
                  auto add_step = [&](int dr, int df) {
                      if (atomIsTuple && !hopper && !rider)
                          tupleV.emplace_back(dr, df);
                      else
                          v[Direction(dr * FILE_NB + df)] = distance;
                  };
                  if (directions.size() == 0 || has_dir("ff") || has_dir("vv") || has_dir("rf") || has_dir("rv") || has_dir("fh") || has_dir("rh") || (has_dir("hr") && !standaloneH))
                      add_step(atom.first, atom.second);
                  if (directions.size() == 0 || has_dir("bb") || has_dir("vv") || has_dir("lb") || has_dir("lv") || has_dir("bh") || has_dir("lh") || (has_dir("hr") && !standaloneH))
                      add_step(-atom.first, -atom.second);
                  if (directions.size() == 0 || has_dir("rr") || has_dir("ss") || has_dir("br") || has_dir("bs") || has_dir("bh") || has_dir("rh") || has_dir("hr"))
                      add_step(-atom.second, atom.first);
                  if (directions.size() == 0 || has_dir("ll") || has_dir("ss") || has_dir("fl") || has_dir("fs") || has_dir("fh") || has_dir("lh") || has_dir("hr"))
                      add_step(atom.second, -atom.first);
                  if (directions.size() == 0 || has_dir("rr") || has_dir("ss") || has_dir("fr") || has_dir("fs") || has_dir("fh") || has_dir("rh") || has_dir("hl"))
                      add_step(atom.second, atom.first);
                  if (directions.size() == 0 || has_dir("ll") || has_dir("ss") || has_dir("bl") || has_dir("bs") || has_dir("bh") || has_dir("lh") || has_dir("hl"))
                      add_step(-atom.second, -atom.first);
                  if (directions.size() == 0 || has_dir("bb") || has_dir("vv") || has_dir("rb") || has_dir("rv") || has_dir("bh") || has_dir("rh") || (has_dir("hl") && !standaloneH))
                      add_step(-atom.first, atom.second);
                  if (directions.size() == 0 || has_dir("ff") || has_dir("vv") || has_dir("lf") || has_dir("lv") || has_dir("fh") || has_dir("lh") || (has_dir("hl") && !standaloneH))
                      add_step(atom.first, -atom.second);
              }
          }
          // Reset per-atom parser state.
          moveModalities.clear();
          prelimDirections.clear();
          hopper = false;
          contraHopper = false;
          rider = false;
          lame = false;
          initial = false;
          dynamicDistance = false;
          skiSlider = false;
          maxDistance = false;
          standaloneH = false;
          distance = 0;
      };

      auto commit_griffon = [&]() {
          // Keep first implementation strict: unqualified O only.
          if (!prelimDirections.empty() || hopper || contraHopper || lame || dynamicDistance || rider)
          {
              moveModalities.clear();
              prelimDirections.clear();
              hopper = false;
              contraHopper = false;
              rider = false;
              lame = false;
              initial = false;
              dynamicDistance = false;
              skiSlider = false;
              maxDistance = false;
              standaloneH = false;
              distance = 0;
              return;
          }
          if (moveModalities.size() == 0)
          {
              moveModalities.push_back(MODALITY_QUIET);
              moveModalities.push_back(MODALITY_CAPTURE);
          }
          for (auto modality : moveModalities)
              p->griffon[initial][modality] = true;
          moveModalities.clear();
          prelimDirections.clear();
          hopper = false;
          contraHopper = false;
          rider = false;
          lame = false;
          initial = false;
          dynamicDistance = false;
          skiSlider = false;
          maxDistance = false;
          standaloneH = false;
          distance = 0;
      };

      auto commit_manticore = [&]() {
          // Keep first implementation strict: unqualified M only.
          if (!prelimDirections.empty() || hopper || contraHopper || lame || dynamicDistance || rider)
          {
              moveModalities.clear();
              prelimDirections.clear();
              hopper = false;
              contraHopper = false;
              rider = false;
              lame = false;
              initial = false;
              dynamicDistance = false;
              skiSlider = false;
              maxDistance = false;
              standaloneH = false;
              distance = 0;
              return;
          }
          if (moveModalities.size() == 0)
          {
              moveModalities.push_back(MODALITY_QUIET);
              moveModalities.push_back(MODALITY_CAPTURE);
          }
          for (auto modality : moveModalities)
              p->manticore[initial][modality] = true;
          moveModalities.clear();
          prelimDirections.clear();
          hopper = false;
          contraHopper = false;
          rider = false;
          lame = false;
          initial = false;
          dynamicDistance = false;
          skiSlider = false;
          maxDistance = false;
          standaloneH = false;
          distance = 0;
      };

      for (std::string::size_type i = 0; i < expandedBetza.size(); i++)
      {
          char c = expandedBetza[i];
          // Modality
          if (c == 'm' || c == 'c')
              moveModalities.push_back(c == 'c' ? MODALITY_CAPTURE : MODALITY_QUIET);
          // Hopper (grasshopper when g)
          else if (c == 'p' || c == 'g')
          {
              hopper = true;
              if (c == 'g')
                  distance = 1;
          }
          // Contra-hopper
          else if (c == 'o')
          {
              contraHopper = true;
              p->add_rider_augment(PieceInfo::AUGMENT_CONTRA);
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
          // Slider ignores friendly pieces
          else if (c == 'y')
              p->friendlyJump = true;
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
                      prelimDirections.push_back(std::string(1, c) + c2);
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
          else if (leaperAtoms.find(c) != leaperAtoms.end() || riderAtoms.find(c) != riderAtoms.end())
          {
              const auto& atoms = riderAtoms.find(c) != riderAtoms.end() ? riderAtoms.find(c)->second
                                                                         : leaperAtoms.find(c)->second;
              commit_atom(atoms, riderAtoms.find(c) != riderAtoms.end(), i, c);
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
          // Griffon bent slider (one orthogonal step, then slide perpendicular)
          else if (c == 'O')
              commit_griffon();
          // Manticore bent slider (one diagonal step, then rook-like slide)
          else if (c == 'M')
              commit_manticore();
          // Tuple leaper atom: (x,y)
          else if (c == '(')
          {
              // Tuple atoms are only supported as explicit leapers. Reject
              // tuple+hoppers/riders to avoid Direction-based wrap artifacts.
              if (hopper || contraHopper || rider || lame || dynamicDistance)
              {
                  std::cerr << "Unsupported Betza tuple modifier combination in '" << betza
                            << "': tuple atoms only support explicit leapers. Ignoring tuple atom." << std::endl;
                  moveModalities.clear();
                  prelimDirections.clear();
                  hopper = false;
                  contraHopper = false;
                  rider = false;
                  lame = false;
                  initial = false;
                  dynamicDistance = false;
                  skiSlider = false;
                  maxDistance = false;
                  standaloneH = false;
                  distance = 0;
                  auto closeUnsupported = expandedBetza.find(')', i + 1);
                  if (closeUnsupported != std::string::npos)
                      i = closeUnsupported;
                  continue;
              }
              auto close = expandedBetza.find(')', i + 1);
              if (close == std::string::npos)
                  continue;
              auto comma = expandedBetza.find(',', i + 1);
              if (comma == std::string::npos || comma > close)
              {
                  i = close;
                  continue;
              }
              int dx = 0, dy = 0;
              if (!parse_positive_int(expandedBetza.substr(i + 1, comma - i - 1), dx)
                  || !parse_positive_int(expandedBetza.substr(comma + 1, close - comma - 1), dy))
              {
                  i = close;
                  continue;
              }
              // Tuple atoms are stored as (rankDelta, fileDelta).
              if ((dx == 0 && dy == 0) || dx > int(RANK_MAX) || dy > int(FILE_MAX))
              {
                  i = close;
                  continue;
              }
              std::vector<std::pair<int, int>> tupleAtom = { std::make_pair(dx, dy) };
              i = close;
              commit_atom(tupleAtom, false, i, ')', true);
          }
      }
      return p;
  }
  // Special multi-leg betza description for Janggi elephant
  PieceInfo* janggi_elephant_piece() {
      PieceInfo* p = from_betza("nZ", "janggiElephant");
      p->betza = "mafsmafW"; // for compatibility with XBoard/Winboard
      return p;
  }
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
      add(pt, from_betza(v != nullptr ? v->customPiece[pt - CUSTOM_PIECES] : "", ""));
}

void PieceMap::add(PieceType pt, const PieceInfo* p) {
  insert(std::pair<PieceType, const PieceInfo*>(pt, p));
}

void PieceMap::clear_all() {
  for (auto const& element : *this)
      delete element.second;
  clear();
}

} // namespace Stockfish
