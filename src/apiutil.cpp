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

#include "apiutil.h"

#include <iomanip>

namespace Stockfish {

namespace {

std::string json_escape(const std::string& value) {
    std::ostringstream out;
    for (unsigned char c : value) {
        switch (c) {
        case '"': out << "\\\""; break;
        case '\\': out << "\\\\"; break;
        case '\b': out << "\\b"; break;
        case '\f': out << "\\f"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (c < 0x20)
                out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << int(c) << std::dec;
            else
                out << c;
        }
    }
    return out.str();
}

std::string quote(const std::string& value) { return "\"" + json_escape(value) + "\""; }
const char* boolean(bool value) { return value ? "true" : "false"; }

std::string piece_type_name(PieceType pt) {
    static const char* names[] = {
        "none", "pawn", "knight", "bishop", "rook", "queen", "fers", "alfil",
        "fersAlfil", "silver", "aiwok", "bers", "archbishop", "chancellor", "amazon",
        "knibis", "biskni", "kniroo", "rookni", "shogiPawn", "lance", "shogiKnight",
        "gold", "dragonHorse", "clobber", "breakthrough", "immobile", "cannon",
        "janggiCannon", "soldier", "horse", "elephant", "janggiElephant", "banner",
        "wazir", "commoner", "centaur"
    };
    if (pt == KING) return "king";
    if (pt >= CUSTOM_PIECES && pt <= CUSTOM_PIECES_END)
        return "custom" + std::to_string(int(pt - CUSTOM_PIECES + 1));
    if (int(pt) >= 0 && int(pt) < int(sizeof(names) / sizeof(names[0])))
        return names[int(pt)];
    if (pt == ALL_PIECES) return "all";
    return "pieceType" + std::to_string(int(pt));
}

std::string piece_type_json(PieceType pt) { return quote(piece_type_name(pt)); }

std::string variant_piece_type_name(const Variant& v, PieceType pt) {
    // FSX stores a custom royal piece under CUSTOM_PIECES_ROYAL.  The public
    // schema keeps the stable semantic role "king" while customBetza carries
    // the resolved movement definition.
    return is_custom(v.kingType) && pt == v.kingType ? "king" : piece_type_name(pt);
}

std::string variant_piece_type_json(const Variant& v, PieceType pt) {
    return quote(variant_piece_type_name(v, pt));
}

std::string piece_set_json(PieceSet pieces) {
    std::ostringstream out;
    out << '[';
    bool first = true;
    for (int i = 1; i < PIECE_TYPE_NB; ++i) {
        PieceType pt = PieceType(i);
        if (!(pieces & piece_set(pt))) continue;
        if (!first) out << ',';
        first = false;
        out << piece_type_json(pt);
    }
    out << ']';
    return out.str();
}

std::string square_name(Square s) {
    return std::string(1, char('a' + file_of(s))) + std::to_string(int(rank_of(s)) + 1);
}

std::string region_json(Bitboard region, File maxFile, Rank maxRank) {
    std::ostringstream out;
    out << '[';
    bool first = true;
    for (int r = 0; r <= int(maxRank); ++r)
        for (int f = 0; f <= int(maxFile); ++f) {
            Square sq = make_square(File(f), Rank(r));
            if (!(region & sq)) continue;
            if (!first) out << ',';
            first = false;
            out << quote(square_name(sq));
        }
    out << ']';
    return out.str();
}

std::string value_name(Value value) {
    if (value == VALUE_NONE) return "none";
    if (value == VALUE_DRAW) return "draw";
    if (value >= VALUE_MATE_IN_MAX_PLY) return "win";
    if (value <= VALUE_MATED_IN_MAX_PLY) return "loss";
    return std::to_string(int(value));
}

const char* enclosing_name(EnclosingRule v) {
    static const char* names[] = {"none", "reversi", "ataxx", "quadwrangle", "snort", "anySide", "top"};
    return names[int(v)];
}
const char* trap_protection_name(TrapProtection v) {
    return v == TrapProtection::FRIENDLY_ORTHOGONAL ? "friendly-orthogonal" : "none";
}
const char* walling_name(WallingRule v) {
    static const char* names[] = {"none", "arrow", "duck", "edge", "past", "static"};
    return names[int(v)];
}
const char* chasing_name(ChasingRule v) { return v == AXF_CHASING ? "axf" : "none"; }
const char* material_counting_name(MaterialCounting v) {
    static const char* names[] = {"none", "janggi", "unweighted", "whiteDrawOdds", "blackDrawOdds"};
    return names[int(v)];
}
const char* counting_name(CountingRule v) {
    static const char* names[] = {"none", "makruk", "cambodian", "asean"};
    return names[int(v)];
}
const char* transfer_side_name(TransferSide v) {
    static const char* names[] = {"us", "them", "owner", "nonOwner"};
    return names[int(v)];
}

void field(std::ostringstream& out, bool& first, const char* name, const std::string& value) {
    if (!first) out << ',';
    first = false;
    out << quote(name) << ':' << value;
}

std::string color_piece_sets(PieceSet white, PieceSet black) {
    return std::string("{\"white\":") + piece_set_json(white) + ",\"black\":" + piece_set_json(black) + '}';
}
std::string color_piece_types(PieceType white, PieceType black) {
    return std::string("{\"white\":") + piece_type_json(white) + ",\"black\":" + piece_type_json(black) + '}';
}
std::string variant_color_piece_types(const Variant& v, PieceType white, PieceType black) {
    return std::string("{\"white\":") + variant_piece_type_json(v, white)
         + ",\"black\":" + variant_piece_type_json(v, black) + '}';
}
std::string color_regions(Bitboard white, Bitboard black, File maxFile, Rank maxRank) {
    return std::string("{\"white\":") + region_json(white, maxFile, maxRank) + ",\"black\":" + region_json(black, maxFile, maxRank) + '}';
}
std::string color_bools(bool white, bool black) {
    return std::string("{\"white\":") + boolean(white) + ",\"black\":" + boolean(black) + '}';
}
std::string color_ints(int white, int black) {
    return std::string("{\"white\":") + std::to_string(white) + ",\"black\":" + std::to_string(black) + '}';
}

std::string piece_type_group_json(const PieceTypeBitboardGroup& group, const Variant& v) {
    std::ostringstream out;
    out << "{\"default\":" << region_json(group.boardOfPiece('*'), v.maxFile, v.maxRank);
    if (group.anySet()) {
        out << ",\"byPiece\":{";
        bool first = true;
        for (int i = 1; i < PIECE_TYPE_NB; ++i) {
            PieceType pt = PieceType(i);
            if (!(v.pieceTypes & piece_set(pt))) continue;
            char pieceChar = v.pieceToChar[make_piece(WHITE, pt)];
            pieceChar = char(std::toupper(static_cast<unsigned char>(pieceChar)));
            if (pieceChar < 'A' || pieceChar > 'Z') continue;
            if (!first) out << ',';
            first = false;
            out << quote(variant_piece_type_name(v, pt)) << ':'
                << region_json(group.boardOfPiece(pieceChar), v.maxFile, v.maxRank);
        }
        out << '}';
    }
    out << '}';
    return out.str();
}

std::string color_piece_type_groups(const Variant& v,
                                    const ColorSetting<PieceTypeBitboardGroup>& groups) {
    return std::string("{\"white\":") + piece_type_group_json(groups[WHITE], v)
         + ",\"black\":" + piece_type_group_json(groups[BLACK], v) + '}';
}

std::string file_piece_set_map_json(const FilePieceSetMap& map, File maxFile) {
    std::ostringstream out;
    out << "{\"default\":" << piece_set_json(map.piecesOfFile(FILE_NB));
    out << ",\"byFile\":{";
    bool first = true;
    for (int f = FILE_A; f <= int(maxFile); ++f) {
        File file = File(f);
        if (map.piecesOfFile(file) == map.piecesOfFile(FILE_NB)) continue;
        if (!first) out << ',';
        first = false;
        out << quote(std::string(1, char('a' + f))) << ':' << piece_set_json(map.piecesOfFile(file));
    }
    out << "}}";
    return out.str();
}

std::string color_file_piece_set_maps(const Variant& v,
                                      const ColorSetting<FilePieceSetMap>& maps) {
    return std::string("{\"white\":") + file_piece_set_map_json(maps[WHITE], v.maxFile)
         + ",\"black\":" + file_piece_set_map_json(maps[BLACK], v.maxFile) + '}';
}

std::string castling_rights_json(CastlingRights rights) {
    return std::string("{\"white\":{\"kingSide\":")
         + boolean(bool(rights & WHITE_OO))
         + ",\"queenSide\":" + boolean(bool(rights & WHITE_OOO))
         + "},\"black\":{\"kingSide\":" + boolean(bool(rights & BLACK_OO))
         + ",\"queenSide\":" + boolean(bool(rights & BLACK_OOO)) + "}}";
}

} // namespace

std::string variant_info_json(const std::string& name) {
    auto it = variants.find(name);
    if (it == variants.end())
        return "";

    const Variant& v = *it->second;
    std::ostringstream out;
    out << '{';
    bool first = true;
    field(out, first, "schemaVersion", "1");
    field(out, first, "name", quote(name));
    field(out, first, "template", quote(v.variantTemplate));

    std::ostringstream board;
    board << '{'; bool b = true;
    field(board, b, "width", std::to_string(int(v.maxFile) + 1));
    field(board, b, "height", std::to_string(int(v.maxRank) + 1));
    field(board, b, "startFen", quote(v.startFen));
    field(board, b, "chess960", boolean(v.chess960));
    field(board, b, "twoBoards", boolean(v.twoBoards));
    field(board, b, "hex", boolean(v.hexBoard));
    field(board, b, "cylindrical", boolean(v.cylindrical));
    field(board, b, "toroidal", boolean(v.toroidal));
    field(board, b, "diagonalLines", region_json(v.diagonalLines, v.maxFile, v.maxRank));
    board << '}'; field(out, first, "board", board.str());

    std::ostringstream pieces;
    pieces << '['; bool pf = true;
    for (int i = 1; i < PIECE_TYPE_NB; ++i) {
        PieceType pt = PieceType(i);
        if (!(v.pieceTypes & piece_set(pt))) continue;
        if (!pf)
            pieces << ',';
        pf = false;
        pieces << "{\"type\":" << variant_piece_type_json(v, pt)
               << ",\"fen\":{\"white\":" << quote(std::string(1, v.pieceToChar[make_piece(WHITE, pt)]))
               << ",\"black\":" << quote(std::string(1, v.pieceToChar[make_piece(BLACK, pt)])) << '}';
        char ws = v.pieceToCharSynonyms[make_piece(WHITE, pt)];
        char bs = v.pieceToCharSynonyms[make_piece(BLACK, pt)];
        pieces << ",\"synonym\":";
        if (ws == ' ' && bs == ' ') pieces << "null";
        else pieces << "{\"white\":" << quote(std::string(1, ws)) << ",\"black\":" << quote(std::string(1, bs)) << '}';
        pieces << ",\"customBetza\":";
        if (pt == KING && is_custom(v.kingType))
            pieces << quote(v.customPiece[v.kingType - CUSTOM_PIECES]);
        else if (is_custom(pt))
            pieces << quote(v.customPiece[pt - CUSTOM_PIECES]);
        else
            pieces << "null";
        pieces << ",\"value\":{\"midgame\":" << v.pieceValue[MG][pt]
               << ",\"endgame\":" << v.pieceValue[EG][pt] << "}}";
    }
    pieces << ']'; field(out, first, "pieces", pieces.str());
    field(out, first, "pieceTypes", piece_set_json(v.pieceTypes));
    PieceSet royalPieceTypes = (v.pieceTypes & piece_set(KING))
                                | v.pseudoRoyalTypes
                                | (v.extinctionPseudoRoyal ? v.extinctionPieceTypes.global : NO_PIECE_SET);
    if (is_custom(v.kingType) && (v.pieceTypes & piece_set(v.kingType)))
        royalPieceTypes |= KING;
    field(out, first, "royalPieceTypes", piece_set_json(royalPieceTypes));

    std::ostringstream movement; movement << '{'; b = true;
    std::ostringstream mobility; mobility << '{'; bool mf = true;
    for (int i = 1; i < PIECE_TYPE_NB; ++i) {
        PieceType pt = PieceType(i);
        if (!(v.pieceTypes & piece_set(pt))) continue;
        std::string entry = std::string("{\"white\":") + region_json(v.mobilityRegion[WHITE][pt], v.maxFile, v.maxRank)
                          + ",\"black\":" + region_json(v.mobilityRegion[BLACK][pt], v.maxFile, v.maxRank) + '}';
        field(mobility, mf, variant_piece_type_name(v, pt).c_str(), entry);
    }
    mobility << '}'; field(movement, b, "mobilityRegions", mobility.str());
    field(movement, b, "doubleStep", boolean(v.doubleStep));
    field(movement, b, "doubleStepRegions", color_regions(v.doubleStepRegion[WHITE], v.doubleStepRegion[BLACK], v.maxFile, v.maxRank));
    field(movement, b, "doubleStepRegionsByPiece", color_piece_type_groups(v, v.doubleStepRegion));
    field(movement, b, "tripleStepRegions", color_regions(v.tripleStepRegion[WHITE], v.tripleStepRegion[BLACK], v.maxFile, v.maxRank));
    field(movement, b, "tripleStepRegionsByPiece", color_piece_type_groups(v, v.tripleStepRegion));
    field(movement, b, "enPassantRegions", color_regions(v.enPassantRegion[WHITE], v.enPassantRegion[BLACK], v.maxFile, v.maxRank));
    field(movement, b, "enPassantTypes", color_piece_sets(v.enPassantTypes[WHITE], v.enPassantTypes[BLACK]));
    field(movement, b, "pass", color_bools(v.pass[WHITE], v.pass[BLACK]));
    field(movement, b, "passOnStalemate", color_bools(v.passOnStalemate[WHITE], v.passOnStalemate[BLACK]));
    field(movement, b, "mustCapture", boolean(v.mustCapture));
    field(movement, b, "immobilityIllegal", boolean(v.immobilityIllegal));
    field(movement, b, "freezePieceTypes", piece_set_json(v.freezePieceTypes));
    field(movement, b, "freezeImmunePieceTypes", piece_set_json(v.freezeImmunePieceTypes));
    field(movement, b, "freezeDiagonals", boolean(v.freezeDiagonals));
    field(movement, b, "cambodianMoves", boolean(v.cambodianMoves));
    field(movement, b, "makpongRule", boolean(v.makpongRule));
    field(movement, b, "flyingGeneral", boolean(v.flyingGeneral));
    field(movement, b, "soldierPromotionRank", std::to_string(int(v.soldierPromotionRank) + 1));
    movement << '}'; field(out, first, "movement", movement.str());

    std::ostringstream promotion; promotion << '{'; b = true;
    field(promotion, b, "regions", color_regions(v.promotionRegion[WHITE], v.promotionRegion[BLACK], v.maxFile, v.maxRank));
    field(promotion, b, "regionsByPiece", color_piece_type_groups(v, v.promotionRegion));
    field(promotion, b, "mandatoryRegions", color_regions(v.mandatoryPromotionRegion[WHITE], v.mandatoryPromotionRegion[BLACK], v.maxFile, v.maxRank));
    field(promotion, b, "mainPawnTypes", color_piece_types(v.mainPromotionPawnType[WHITE], v.mainPromotionPawnType[BLACK]));
    field(promotion, b, "pawnTypes", color_piece_sets(v.promotionPawnTypes[WHITE], v.promotionPawnTypes[BLACK]));
    field(promotion, b, "pieceTypes", color_piece_sets(v.promotionPieceTypes[WHITE], v.promotionPieceTypes[BLACK]));
    field(promotion, b, "pieceTypesByFile", color_file_piece_set_maps(v, v.promotionPieceTypes));
    std::ostringstream promoted; promoted << '{'; bool prf = true;
    for (int i = 1; i < PIECE_TYPE_NB; ++i) if (v.promotedPieceType[i] != NO_PIECE_TYPE)
        field(promoted, prf, variant_piece_type_name(v, PieceType(i)).c_str(), variant_piece_type_json(v, v.promotedPieceType[i]));
    promoted << '}'; field(promotion, b, "promotedPieceTypes", promoted.str());
    std::ostringstream limits; limits << '{'; bool lf = true;
    for (int i = 1; i < PIECE_TYPE_NB; ++i) if (v.promotionLimit[i])
        field(limits, lf, variant_piece_type_name(v, PieceType(i)).c_str(), std::to_string(v.promotionLimit[i]));
    limits << '}'; field(promotion, b, "limits", limits.str());
    field(promotion, b, "sittuyin", boolean(v.sittuyinPromotion));
    field(promotion, b, "onCapture", boolean(v.piecePromotionOnCapture));
    field(promotion, b, "mandatoryPawn", boolean(v.mandatoryPawnPromotion));
    field(promotion, b, "mandatoryPiece", boolean(v.mandatoryPiecePromotion));
    field(promotion, b, "demotion", boolean(v.pieceDemotion));
    field(promotion, b, "shogiStyle", boolean(v.shogiStylePromotions));
    field(promotion, b, "steal", boolean(v.promotionSteal));
    field(promotion, b, "requireInHand", boolean(v.promotionRequireInHand));
    field(promotion, b, "consumeInHand", boolean(v.promotionConsumeInHand));
    promotion << '}'; field(out, first, "promotion", promotion.str());

    std::ostringstream capture; capture << '{'; b = true;
    field(capture, b, "blast", boolean(v.blastOnCapture));
    field(capture, b, "blastOnMove", boolean(v.blastOnMove));
    field(capture, b, "blastOnSelfDestruct", boolean(v.blastOnSelfDestruct));
    field(capture, b, "blastPattern", quote(v.blastPattern));
    field(capture, b, "blastCenter", boolean(v.blastCenter));
    field(capture, b, "blastDiagonals", boolean(v.blastDiagonals));
    field(capture, b, "captureMorph", boolean(v.captureMorph));
    field(capture, b, "selfDestructTypes", piece_set_json(v.selfDestructTypes));
    field(capture, b, "blastImmuneTypes", piece_set_json(v.blastImmuneTypes));
    field(capture, b, "mutuallyImmuneTypes", piece_set_json(v.mutuallyImmuneTypes));
    field(capture, b, "petrifyTypes", piece_set_json(v.petrifyOnCaptureTypes));
    field(capture, b, "petrifyBlastPieces", boolean(v.petrifyBlastPieces));
    field(capture, b, "petrifySuppressTransfer", boolean(v.petrifyOnCaptureSuppressTransfer));
    field(capture, b, "trapRegion", region_json(v.trapRegion, v.maxFile, v.maxRank));
    field(capture, b, "trapProtection", quote(trap_protection_name(v.trapProtection)));
    field(capture, b, "rifle", boolean(v.rifleCapture));
    field(capture, b, "selfCapture", color_bools(v.selfCapture[WHITE], v.selfCapture[BLACK]));
    field(capture, b, "selfCaptureTypes", color_piece_sets(v.selfCaptureTypes[WHITE], v.selfCaptureTypes[BLACK]));
    capture << '}'; field(out, first, "capture", capture.str());

    std::ostringstream castling; castling << '{'; b = true;
    field(castling, b, "enabled", boolean(v.castling));
    field(castling, b, "droppedPiece", boolean(v.castlingDroppedPiece));
    field(castling, b, "kingSideFile", std::to_string(int(v.castlingKingsideFile) + 1));
    field(castling, b, "queenSideFile", std::to_string(int(v.castlingQueensideFile) + 1));
    field(castling, b, "rank", std::to_string(int(v.castlingRank) + 1));
    field(castling, b, "kingFile", std::to_string(int(v.castlingKingFile) + 1));
    field(castling, b, "rookKingSideFile", std::to_string(int(v.castlingRookKingsideFile) + 1));
    field(castling, b, "rookQueenSideFile", std::to_string(int(v.castlingRookQueensideFile) + 1));
    field(castling, b, "kingPieces", variant_color_piece_types(v, v.castlingKingPiece[WHITE], v.castlingKingPiece[BLACK]));
    field(castling, b, "rookPieces", color_piece_sets(v.castlingRookPieces[WHITE], v.castlingRookPieces[BLACK]));
    field(castling, b, "opposite", boolean(v.oppositeCastling));
    field(castling, b, "wins", castling_rights_json(v.castlingWins));
    castling << '}'; field(out, first, "castling", castling.str());

    std::ostringstream drops; drops << '{'; b = true;
    field(drops, b, "enabled", boolean(v.pieceDrops));
    field(drops, b, "capturesToHand", boolean(v.captureType != MOVE_OUT));
    field(drops, b, "mustDrop", boolean(v.mustDrop));
    field(drops, b, "mustDropType", piece_type_json(v.mustDropType));
    field(drops, b, "dropLoop", boolean(v.dropLoop));
    field(drops, b, "firstRankPawnDrops", boolean(v.firstRankPawnDrops));
    field(drops, b, "promotionZonePawnDrops", boolean(v.promotionZonePawnDrops));
    field(drops, b, "regions", color_regions(v.dropRegion[WHITE], v.dropRegion[BLACK], v.maxFile, v.maxRank));
    field(drops, b, "regionsByPiece", color_piece_type_groups(v, v.dropRegion));
    field(drops, b, "enclosingRule", quote(enclosing_name(v.enclosingDrop)));
    field(drops, b, "enclosingStart", region_json(v.enclosingDropStart, v.maxFile, v.maxRank));
    field(drops, b, "sittuyinRook", boolean(v.sittuyinRookDrop));
    field(drops, b, "oppositeColoredBishop", boolean(v.dropOppositeColoredBishop));
    field(drops, b, "promoted", boolean(v.dropPromoted));
    field(drops, b, "noDoubledTypes", color_piece_sets(v.dropNoDoubled[WHITE], v.dropNoDoubled[BLACK]));
    field(drops, b, "noDoubledCount", color_ints(v.dropNoDoubledCount[WHITE], v.dropNoDoubledCount[BLACK]));
    field(drops, b, "free", boolean(v.freeDrops));
    field(drops, b, "captureType", quote(v.captureType == HAND ? "hand" : (v.captureType == PRISON ? "prison" : "move")));
    field(drops, b, "captureToHandSide", quote(transfer_side_name(v.captureToHandSide)));
    field(drops, b, "captureToHandTypes", piece_set_json(v.captureToHandTypes));
    drops << '}'; field(out, first, "drops", drops.str());

    std::ostringstream gating; gating << '{'; b = true;
    field(gating, b, "enabled", boolean(v.gating));
    field(gating, b, "seirawan", boolean(v.seirawanGating));
    field(gating, b, "wallingRule", quote(walling_name(v.wallingRule)));
    field(gating, b, "wallingRegions", color_regions(v.wallingRegion[WHITE], v.wallingRegion[BLACK], v.maxFile, v.maxRank));
    field(gating, b, "wallOrMove", boolean(v.wallOrMove));
    gating << '}'; field(out, first, "gating", gating.str());

    std::ostringstream end; end << '{'; b = true;
    field(end, b, "checking", boolean(v.checking));
    field(end, b, "allowChecks", boolean(v.allowChecks));
    field(end, b, "dropChecks", boolean(v.dropChecks));
    field(end, b, "dropChecksByColor", color_bools(v.dropChecks[WHITE], v.dropChecks[BLACK]));
    field(end, b, "mustCapture", boolean(v.mustCapture));
    field(end, b, "mustCaptureByColor", color_bools(v.mustCapture[WHITE], v.mustCapture[BLACK]));
    field(end, b, "kingType", variant_piece_type_json(v, v.kingType));
    field(end, b, "nMoveRule", std::to_string(v.nMoveRule));
    field(end, b, "nMoveRuleTypes", color_piece_sets(v.nMoveRuleTypes[WHITE], v.nMoveRuleTypes[BLACK]));
    field(end, b, "nFoldRule", std::to_string(v.nFoldRule));
    field(end, b, "nFoldValue", quote(value_name(v.nFoldValue)));
    field(end, b, "nFoldValueAbsolute", boolean(v.nFoldValueAbsolute));
    field(end, b, "perpetualCheckIllegal", boolean(v.perpetualCheckIllegal));
    field(end, b, "moveRepetitionIllegal", boolean(v.moveRepetitionIllegal));
    field(end, b, "chasingRule", quote(chasing_name(v.chasingRule)));
    field(end, b, "stalemateValue", quote(value_name(v.stalemateValue)));
    field(end, b, "stalematePieceCount", boolean(v.stalematePieceCount));
    field(end, b, "checkmateValue", quote(value_name(v.checkmateValue)));
    field(end, b, "shogiPawnDropMateIllegal", boolean(v.shogiPawnDropMateIllegal));
    field(end, b, "shatarMateRule", boolean(v.shatarMateRule));
    field(end, b, "bikjangRule", boolean(v.bikjangRule));
    field(end, b, "dupleCheck", boolean(v.dupleCheck));
    field(end, b, "checkCounting", boolean(v.checkCounting));
    field(end, b, "materialCounting", quote(material_counting_name(v.materialCounting)));
    field(end, b, "adjudicateFullBoard", boolean(v.adjudicateFullBoard));
    field(end, b, "countingRule", quote(counting_name(v.countingRule)));
    end << '}'; field(out, first, "gameEnd", end.str());

    std::ostringstream extinction; extinction << '{'; b = true;
    field(extinction, b, "value", quote(value_name(v.extinctionValue)));
    field(extinction, b, "valueByColor", color_ints(int(v.extinctionValue[WHITE]), int(v.extinctionValue[BLACK])));
    field(extinction, b, "claim", boolean(v.extinctionClaim));
    field(extinction, b, "pseudoRoyal", boolean(v.extinctionPseudoRoyal));
    field(extinction, b, "pseudoRoyalTypes", piece_set_json(v.pseudoRoyalTypes));
    field(extinction, b, "pseudoRoyalCount", std::to_string(v.pseudoRoyalCount));
    field(extinction, b, "pseudoRoyalValue", quote(value_name(v.pseudoRoyalValue)));
    field(extinction, b, "pseudoRoyalCaptureIllegal", boolean(v.pseudoRoyalCaptureIllegal));
    field(extinction, b, "antiRoyalTypes", piece_set_json(v.antiRoyalTypes));
    field(extinction, b, "antiRoyalCount", std::to_string(v.antiRoyalCount));
    field(extinction, b, "antiRoyalSelfCaptureOnly", boolean(v.antiRoyalSelfCaptureOnly));
    field(extinction, b, "pieceTypes", piece_set_json(v.extinctionPieceTypes));
    field(extinction, b, "pieceTypesByColor", color_piece_sets(v.extinctionPieceTypes[WHITE], v.extinctionPieceTypes[BLACK]));
    field(extinction, b, "allPieceTypesByColor", color_bools(v.extinctionAllPieceTypes[WHITE], v.extinctionAllPieceTypes[BLACK]));
    field(extinction, b, "pieceCount", std::to_string(v.extinctionPieceCount));
    field(extinction, b, "opponentPieceCount", std::to_string(v.extinctionOpponentPieceCount));
    field(extinction, b, "pieceCountByColor", color_ints(v.extinctionPieceCount[WHITE], v.extinctionPieceCount[BLACK]));
    field(extinction, b, "opponentPieceCountByColor", color_ints(v.extinctionOpponentPieceCount[WHITE], v.extinctionOpponentPieceCount[BLACK]));
    extinction << '}'; field(out, first, "extinction", extinction.str());

    std::ostringstream flag; flag << '{'; b = true;
    field(flag, b, "pieces", color_piece_sets(v.flagPieceTypes[WHITE], v.flagPieceTypes[BLACK]));
    field(flag, b, "regions", color_regions(v.flagRegion[WHITE], v.flagRegion[BLACK], v.maxFile, v.maxRank));
    field(flag, b, "pieceCount", std::to_string(v.flagPieceCount));
    field(flag, b, "blockedWin", boolean(v.flagPieceBlockedWin));
    field(flag, b, "move", boolean(v.flagMove));
    field(flag, b, "safe", boolean(v.flagPieceSafe));
    flag << '}'; field(out, first, "flag", flag.str());

    std::ostringstream connect; connect << '{'; b = true;
    field(connect, b, "n", std::to_string(v.connectN));
    field(connect, b, "pieceTypes", piece_set_json(v.connectPieceTypes));
    field(connect, b, "horizontal", boolean(v.connectHorizontal));
    field(connect, b, "vertical", boolean(v.connectVertical));
    field(connect, b, "diagonal", boolean(v.connectDiagonal));
    field(connect, b, "northEast", boolean(v.connectNorthEast));
    field(connect, b, "southEast", boolean(v.connectSouthEast));
    field(connect, b, "threeDimensional", boolean(v.connect3D));
    field(connect, b, "fourDimensional", boolean(v.connect4D));
    field(connect, b, "region1", color_regions(v.connectRegion1[WHITE], v.connectRegion1[BLACK], v.maxFile, v.maxRank));
    field(connect, b, "region2", color_regions(v.connectRegion2[WHITE], v.connectRegion2[BLACK], v.maxFile, v.maxRank));
    field(connect, b, "nxn", std::to_string(v.connectNxN));
    field(connect, b, "group", std::to_string(v.connectGroup));
    field(connect, b, "collinearN", std::to_string(v.collinearN));
    field(connect, b, "value", quote(value_name(v.connectValue)));
    connect << '}'; field(out, first, "connect", connect.str());

    std::ostringstream enclosing; enclosing << '{'; b = true;
    field(enclosing, b, "flipRule", quote(enclosing_name(v.flipEnclosedPieces)));
    enclosing << '}'; field(out, first, "enclosing", enclosing.str());

    std::ostringstream protocol; protocol << '{'; b = true;
    field(protocol, b, "pieceToCharTable", quote(v.pieceToCharTable));
    field(protocol, b, "pocketSize", std::to_string(v.pocketSize));
    field(protocol, b, "pieceToChar", quote(v.pieceToChar));
    protocol << '}'; field(out, first, "protocol", protocol.str());

    out << '}';
    return out.str();
}

} // namespace Stockfish
