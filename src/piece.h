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

#ifndef PIECE_H_INCLUDED
#define PIECE_H_INCLUDED

#include <cassert>
#include <array>
#include <string>
#include <map>
#include <vector>

#include "types.h"
#include "variant.h"

namespace Stockfish {

enum MoveModality {MODALITY_QUIET, MODALITY_CAPTURE, MOVE_MODALITY_NB};

// Special distance value for dynamic slider length (Betza 'x' modifier)
constexpr int DYNAMIC_SLIDER_LIMIT = -2;
// Special distance value for ski/slip sliders (Betza 'j' modifier)
constexpr int SKI_SLIDER_LIMIT = -3;
// Special distance value for max-distance sliders (Betza 'z' modifier)
constexpr int MAX_SLIDER_LIMIT = -4;
// Encoded bounded/open-ended slider range for bracketed Betza syntax.
constexpr int SLIDER_RANGE_FLAG = 1 << 29;

inline bool is_slider_range(int limit) {
  return limit > 0 && (limit & SLIDER_RANGE_FLAG);
}

inline int encode_slider_range(int minDistance, int maxDistance) {
  assert(minDistance > 0);
  assert(maxDistance >= 0 && maxDistance <= 255);
  assert(minDistance <= 255);
  return SLIDER_RANGE_FLAG | (minDistance << 8) | maxDistance;
}

inline int slider_min_distance(int limit) {
  return is_slider_range(limit) ? ((limit >> 8) & 0xFF) : (limit == SKI_SLIDER_LIMIT ? 2 : 1);
}

inline int slider_max_distance(int limit) {
  return is_slider_range(limit) ? (limit & 0xFF) : (limit > 0 ? limit : 0);
}

/// PieceInfo struct stores information about the piece movements.

struct PieceInfo {
  enum Augment : uint8_t {
    AUGMENT_NONE = 0,
    AUGMENT_DYNAMIC = 1 << 0,
    AUGMENT_MAX = 1 << 1
  };

  struct TupleRay {
    int dr;
    int df;
    int limit;
  };

  enum CaptureMode {
    CAPTURE_DEST,
    CAPTURE_LOCUST_ALL,
    CAPTURE_LOCUST_FIRST,
    CAPTURE_LOCUST_LAST
  };

  enum EquiRule {
    EQUI_NONE,
    EQUI_HOPPER,
    EQUI_STOPPER
  };

  struct HopperProfile {
    enum SpecialType : uint8_t {
      NONE = 0,
      ENEMY = 1 << 0,
      FRIENDLY = 1 << 1,
      WALL = 1 << 2,
      DEAD = 1 << 3,
      ALL_SPECIAL = ENEMY | FRIENDLY | WALL | DEAD
    };

    int hurdlesMin = 1;
    int hurdlesMax = 1;
    int preMin = 1;
    int preMax = 255;
    int postMin = 1;
    int postMax = 1;
    CaptureMode captureMode = CAPTURE_DEST;
    EquiRule equiRule = EQUI_NONE;
    PieceSet hurdlePieceTypes = PieceSet(0);
    PieceSet transparentPieceTypes = PieceSet(0);
    uint8_t hurdleSpecialTypes = ENEMY | FRIENDLY;
    uint8_t transparentSpecialTypes = NONE;
  };

  struct LameProfile {
    enum PathType : uint8_t {
      ORTH_FIRST,
      DIAG_FIRST,
      ANY_PATH,
      MIDPOINT
    };

    PathType path = MIDPOINT;
    // -1 = single leap, 0 = unlimited rider, positive = maximum rider hops.
    int limit = -1;
  };

  std::string name = "";
  std::string betza = "";
  std::map<Direction, int> steps[2][MOVE_MODALITY_NB] = {};
  std::map<Direction, LameProfile> stepsLame[2][MOVE_MODALITY_NB] = {};
  std::vector<std::pair<int, int>> tupleSteps[2][MOVE_MODALITY_NB] = {};
  std::vector<TupleRay> tupleSlider[2][MOVE_MODALITY_NB] = {};
  std::map<Direction, int> slider[2][MOVE_MODALITY_NB] = {};
  std::map<Direction, int> leapRider[2][MOVE_MODALITY_NB] = {};
  std::map<Direction, int> hopper[2][MOVE_MODALITY_NB] = {};
  std::map<Direction, HopperProfile> universalHopper[2][MOVE_MODALITY_NB] = {};
  bool griffon[2][MOVE_MODALITY_NB] = {};
  bool manticore[2][MOVE_MODALITY_NB] = {};
  bool rose[2][MOVE_MODALITY_NB] = {};
  uint8_t riderAugmentMask = AUGMENT_NONE;
  bool rifleCapture = false;
  int mobilityScaling = 100;
  bool diagonalLimitedSlider = false;

  inline void add_rider_augment(Augment augment) { riderAugmentMask |= augment; }
  inline bool has_universal_hopper() const {
    for (int initial = 0; initial < 2; ++initial)
      for (int modality = 0; modality < MOVE_MODALITY_NB; ++modality)
        if (!universalHopper[initial][modality].empty())
          return true;
    return false;
  }
  inline bool has_universal_capture_hopper() const {
    for (int initial = 0; initial < 2; ++initial)
      if (!universalHopper[initial][MODALITY_CAPTURE].empty())
        return true;
    return false;
  }
  inline bool has_lame_leaper() const {
    for (int initial = 0; initial < 2; ++initial)
      for (int modality = 0; modality < MOVE_MODALITY_NB; ++modality)
        if (!stepsLame[initial][modality].empty())
          return true;
    return false;
  }
  inline bool has_lame_capture() const {
    for (int initial = 0; initial < 2; ++initial)
      if (!stepsLame[initial][MODALITY_CAPTURE].empty())
        return true;
    return false;
  }
  inline bool has_runtime_rider_augment() const { return riderAugmentMask != AUGMENT_NONE || has_universal_hopper() || has_lame_leaper(); }
  inline bool has_dynamic_slider() const { return riderAugmentMask & AUGMENT_DYNAMIC; }
  inline bool has_max_slider() const { return riderAugmentMask & AUGMENT_MAX; }
  inline bool has_explicit_initial_moves() const {
    for (int modality = 0; modality < MOVE_MODALITY_NB; ++modality)
      if (!steps[1][modality].empty()
          || !stepsLame[1][modality].empty()
          || !tupleSteps[1][modality].empty()
          || !tupleSlider[1][modality].empty()
          || !slider[1][modality].empty()
          || !leapRider[1][modality].empty()
          || !hopper[1][modality].empty()
          || !universalHopper[1][modality].empty()
          || griffon[1][modality]
          || manticore[1][modality]
          || rose[1][modality])
        return true;
    return false;
  }
};

struct PieceMap : public std::map<PieceType, PieceInfo*> {
  PieceMap() {
    direct.fill(nullptr);
    runtimeRiderAugmentTypes = PieceSet(0);
  }
  ~PieceMap() { clear_all(); }
  void init(const Variant* v = nullptr);
  void add(PieceType pt, PieceInfo* v);
  void clear_all();
  const PieceInfo* get(PieceType pt) const {
    assert(pt < PIECE_TYPE_NB);
    assert(direct[pt] != nullptr);
    return direct[pt];
  }
  PieceInfo* get(PieceType pt) {
    assert(pt < PIECE_TYPE_NB);
    assert(direct[pt] != nullptr);
    return direct[pt];
  }
  PieceSet runtime_rider_augment_types() const { return runtimeRiderAugmentTypes; }

private:
  std::array<PieceInfo*, PIECE_TYPE_NB> direct;
  PieceSet runtimeRiderAugmentTypes;
};

extern PieceMap pieceMap;

bool validate_custom_piece_betza(const std::string& betza, const std::string& name, const Variant* variant = nullptr);

inline std::string piece_name(PieceType pt) {
  return is_custom(pt) ? "customPiece" + std::to_string(pt - CUSTOM_PIECES + 1)
                       : pieceMap.get(pt)->name;
}

} // namespace Stockfish

#endif // #ifndef PIECE_H_INCLUDED
