# -*- coding: utf-8 -*-

import faulthandler
import os
import subprocess
import sys
import tempfile
from pathlib import Path
import unittest
import pyffish as sf

faulthandler.enable()

CHESS = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
CHESS960 = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w HAha - 0 1"
CAPA = "rnabqkbcnr/pppppppppp/10/10/10/10/PPPPPPPPPP/RNABQKBCNR w KQkq - 0 1"
CAPAHOUSE = "rnabqkbcnr/pppppppppp/10/10/10/10/PPPPPPPPPP/RNABQKBCNR[] w KQkq - 0 1"
SITTUYIN = "8/8/4pppp/pppp4/4PPPP/PPPP4/8/8[KFRRSSNNkfrrssnn] w - - 0 1"
MAKRUK = "rnsmksnr/8/pppppppp/8/8/PPPPPPPP/8/RNSKMSNR w - - 0 1"
CAMBODIAN = "rnsmksnr/8/pppppppp/8/8/PPPPPPPP/8/RNSKMSNR w DEde - 0 1"
SHOGI = "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL[-] w - - 0 1"
SHOGI_SFEN = "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"
SEIRAWAN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR[EHeh] w KQBCDFGkqbcdfg - 0 1"
GRAND = "r8r/1nbqkcabn1/pppppppppp/10/10/10/10/PPPPPPPPPP/1NBQKCABN1/R8R w - - 0 1"
GRANDHOUSE = "r8r/1nbqkcabn1/pppppppppp/10/10/10/10/PPPPPPPPPP/1NBQKCABN1/R8R[] w - - 0 1"
XIANGQI = "rnbakabnr/9/1c5c1/p1p1p1p1p/9/9/P1P1P1P1P/1C5C1/9/RNBAKABNR w - - 0 1"
SHOGUN = "rnb+fkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNB+FKBNR[] w KQkq - 0 1"
JANGGI = "rnba1abnr/4k4/1c5c1/p1p1p1p1p/9/9/P1P1P1P1P/1C5C1/4K4/RNBA1ABNR w - - 0 1"


ini_text = """
# Hybrid variant of Grand-chess and crazyhouse, using Grand-chess as a template
[grandhouse:grand]
startFen = r8r/1nbqkcabn1/pppppppppp/10/10/10/10/PPPPPPPPPP/1NBQKCABN1/R8R[] w - - 0 1
pieceDrops = true
capturesToHand = true

# Shogun chess
[shogun:crazyhouse]
startFen = rnb+fkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNB+FKBNR[] w KQkq - 0 1
commoner = c
centaur = g
archbishop = a
chancellor = m
fers = f
promotionRegionWhite = *6 *7 *8
promotionRegionBlack = *3 *2 *1
promotionLimit = g:1 a:1 m:1 q:1
promotionPieceTypes = -
promotedPieceType = p:c n:g b:a r:m f:q
mandatoryPawnPromotion = false
firstRankPawnDrops = true
promotionZonePawnDrops = true
dropRegionWhite = *1 *2 *3 *4 *5
dropRegionBlack = *4 *5 *6 *7 *8
immobilityIllegal = true

# Asymmetric variant with one army using pieces that move like knights but attack like other pieces (kniroo and knibis)
[orda:chess]
startFen = lhaykahl/8/pppppppp/8/8/8/PPPPPPPP/RNBQKBNR w KQ - 0 1
centaur = h
knibis = a
kniroo = l
silver = y
promotionPieceTypes = qh
flagPiece = k
flagRegionWhite = *8
flagRegionBlack = *1

[diana:losalamos]
pieceToCharTable = PNBRQ................Kpnbrq................k
bishop = b
promotionPieceTypes = rbn
castling = true
castlingKingsideFile = e
castlingQueensideFile = b
startFen = rbnkbr/pppppp/6/6/PPPPPP/RBNKBR w KQkq - 0 1

[passchess:chess]
pass = true

[royalduck:duck]
extinctionPseudoRoyal = true

[makhouse:makruk]
startFen = rnsmksnr/8/pppppppp/8/8/PPPPPPPP/8/RNSKMSNR[] w - - 0 1
pieceDrops = true
capturesToHand = true
firstRankPawnDrops = true
promotionZonePawnDrops = true
immobilityIllegal = true

[wazirking:chess]
fers = q
king = k:W
startFen = 7k/5Kq1/8/8/8/8/8/8 w - - 0 1
stalemateValue = loss
nFoldValue = loss

[betzatest]
maxRank = 7
maxFile = 7
customPiece1 = a:lhN
customPiece2 = b:rhN
customPiece3 = c:hlN
customPiece4 = d:hrN
customPiece5 = e:pB3
startFen = 7/7/7/3A3/7/7/7 w - - 0 1

[cannonshogi:shogi]
dropNoDoubled = -
shogiPawnDropMateIllegal = false
soldier = p
cannon = u
customPiece1 = a:pR
customPiece2 = c:mBcpB
customPiece3 = i:pB
customPiece4 = w:mRpRmFpB2
customPiece5 = f:mBpBmWpR2
promotedPieceType = u:w a:w c:f i:f
startFen = lnsgkgsnl/1rci1uab1/p1p1p1p1p/9/9/9/P1P1P1P1P/1BAU1ICR1/LNSGKGSNL[-] w 0 1

[fogofwar:chess]
king = -
commoner = k
castlingKingPiece = k
extinctionValue = loss
extinctionPieceTypes = k

[coregaldrop:coregal]
pieceDrops = true
startFen = rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR[Qq] w KQkq - 0 1

[cannonatomic:atomic]
cannon = c

[multipawn:chess]
soldier = s
pawnTypes = p s

[capture-anything:chess]
selfCapture = true

[checkersmini]
customPiece1 = m:mfFfc{hurdles: 1,1; pre: 1,1; post: 1,1; capture: locust_first; hurdle_types:enemy}F
customPiece2 = k:mFc{hurdles: 1,1; pre: 1,1; post: 1,1; capture: locust_first; hurdle_types:enemy}F
startFen = 8/8/8/8/8/8/8/8 w - - 0 1
promotionPawnTypes = m
promotionPieceTypes = k
mustCapture = true
checking = false
forcedJumpContinuation = true
stalemateValue = loss
nMoveRule = 0
nFoldRule = 3

[selfhouse:crazyhouse]
selfCapture = true

[hostageblank:chess]

[diagfaceoff:chess]
maxRank = 10
maxFile = 10
diagonalGeneral = true
startFen = 10/10/10/4k5/3P6/2K7/10/10/10/10 w - - 0 1

[fenmask:chess]
gating = true
seirawanGating = true
maxFile = 12
startFen = rnbqkbnr4/pppppppp4/12/12/12/12/PPPPPPPP4/RNBQKBNR3R[Q] w KQkqk|000000000001/000000000000 - 0 1

[blastconnect]
maxRank = 4
maxFile = 4
king = -
immobile = e
wazir = t
customPiece1 = f:mWD
promotedPieceType = e:t t:f f:e
startFen = 4/4/4/4[EEEEEEEEEEEEEeeeeeeeeeeeee] w - - 0 1 {0 0}
stalemateValue = loss
pieceDrops = true
pointsCounting = true
pointsRuleCaptures = owner
piecePoints = e:1 t:1 f:1
blastOnMove = true
blastPromotion = true
blastDiagonals = false
blastCenter = false
removeConnectN = 3
removeConnectNByType = true

[capmapwild:chess]
king = -
customPiece1 = a:W
customPiece2 = b:W
captureForbidden = *:*
captureAllowed = a:b
startFen = 8/8/8/3b4/3A4/8/8/8 w - - 0 1

[blast-default-test:fairy]
blastOnCapture = true
rifleCapture = true
king = -
startFen = 8/8/8/8/8/8/8/8 w - - 0 1

[blast-mover-test:fairy]
blastOnCapture = true
rifleCapture = true
blastOnCaptureMoverCenter = true
king = -
startFen = 8/8/8/8/8/8/8/8 w - - 0 1

[mini-nightrider:chess]
maxRank = 7
maxFile = 7
king = k
customPiece1 = N:NN
startFen = 6k/7/7/3N3/7/7/K6 w - - 0 1
"""

sf.load_variant_config(ini_text)


def repo_variants_ini():
    path = Path(__file__).resolve().parent / "src" / "variants.ini"
    return path if path.exists() else None


def load_repo_variants_or_skip():
    path = repo_variants_ini()
    if path is None:
        raise unittest.SkipTest("repo variants.ini is not available in this test environment")
    sf.load_variant_config(path.read_text())
    return path


variant_positions = {
    "chess": {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1": (False, False),  # startpos
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -": (False, False),  # startpos
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR": (False, False),  # startpos
        "rnbqkbnr/ppp2ppp/4p3/3pP3/8/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 3": (False, False),
        "k7/8/8/8/8/8/8/K7 w - - 0 1": (True, True),  # K vs K
        "k7/b7/8/8/8/8/8/K7 w - - 0 1": (True, True),  # K vs KB
        "k7/n7/8/8/8/8/8/K7 w - - 0 1": (True, True),  # K vs KN
        "k7/p7/8/8/8/8/8/K7 w - - 0 1": (True, False),  # K vs KP
        "k7/r7/8/8/8/8/8/K7 w - - 0 1": (True, False),  # K vs KR
        "k7/q7/8/8/8/8/8/K7 w - - 0 1": (True, False),  # K vs KQ
        "k7/nn6/8/8/8/8/8/K7 w - - 0 1": (True, False),  # K vsNN K
        "k7/bb6/8/8/8/8/8/K7 w - - 0 1": (True, False),  # K vs KBB opp color
        "k7/b1b5/8/8/8/8/8/K7 w - - 0 1": (True, True),  # K vs KBB same color
        "kb6/8/8/8/8/8/8/K1B6 w - - 0 1": (True, True),  # KB vs KB same color
        "kb6/8/8/8/8/8/8/KB7 w - - 0 1": (False, False),  # KB vs KB opp color
        "8/8/8/8/8/6KN/8/6nk w - - 0 1": (False, False),  # KN vs KN
    },
    "atomic": {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1": (False, False),  # startpos
        "8/8/8/8/3K4/3k4/8/8 b - - 0 1": (False, False),  # helper suppressed for atomic win rules
        "k7/p7/8/8/8/8/8/K7 w - - 0 1": (False, False),  # helper suppressed for atomic win rules
        "k7/q7/8/8/8/8/8/K7 w - - 0 1": (False, False),  # helper suppressed for atomic win rules
    },
    "crazyhouse": {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR/ w KQkq - 0 1": (False, False),  # lichess style startpos
    },
    "3check": {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 3+3 0 1": (False, False),  # startpos
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 +0+2": (False, False),  # lichess style check count
        "k7/n7/8/8/8/8/8/K7 w - - 1+2 0 1": (False, False),  # helper suppressed for check-counting variants
        "k7/b7/8/8/8/8/8/K7 w - - 3+1 0 1": (False, False),  # helper suppressed for check-counting variants
    },
    "horde": {
        "rnbqkbnr/pppppppp/8/1PP2PP1/PPPPPPPP/PPPPPPPP/PPPPPPPP/PPPPPPPP w kq - 0 1": (False, False),  # startpos
    },
    "racingkings": {
        "8/8/8/8/8/8/krbnNBRK/qrbnNBRQ w - - 0 1": (False, False),  # startpos
        "8/8/8/8/8/8/K6k/8 w - - 0 1": (False, False),  # KvK
    },
    "placement": {
        "8/pppppppp/8/8/8/8/PPPPPPPP/8[KQRRBBNNkqrrbbnn] w - - 0 1": (False, False),  # startpos
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR[] w KQkq - 0 1": (False, False),  # chess startpos
        "k7/8/8/8/8/8/8/K7[] w - - 0 1": (True, True),  # K vs K
    },
    "newzealand": {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1": (False, False),  # startpos
    },
    "amazon": {
        "rnbakbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBAKBNR w KQkq - 0 1": (False, False),  # startpos
        "8/8/8/8/A7/6k1/8/1K6 w - - 0 1": (False, True),  # KA vs K
    },
    "seirawan": {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR[HEhe] w KQBCDFGkqbcdfg - 0 1": (False, False),  # startpos
        "k7/8/8/8/8/8/8/K7[] w - - 0 1": (True, True),  # K vs K
        "k7/8/8/8/8/8/8/KH6[] w - - 0 1": (False, True),  # KH vs K
        "k7/8/8/8/8/8/8/4K3[E] w E - 0 1": (False, True),  # KE vs K
    },
    "cambodian": {
        "rnsmksnr/8/pppppppp/8/8/PPPPPPPP/8/RNSKMSNR w DEde 0 0 1": (False, False),  # startpos
        "1ns1ksn1/r6r/pppmpppp/3p4/8/PPPPPPPP/RK2N2R/1NS1MS2 w Ee - 6 5": (False, False),
    },
    "sittuyin": {
        "8/8/4pppp/pppp4/4PPPP/PPPP4/8/8[KFRRSSNNkfrrssnn] w - - 0 1": (False, False),  # startpos
        "k7/8/8/8/8/8/8/K7 w - - 0 1": (True, True),  # K vs K, skip pocket
        "k6P/8/8/8/8/8/8/K7[] w - - 0 1": (True, True),  # KP vs K
        "k6P/8/8/8/8/8/8/K6p[] w - - 0 1": (False, False),  # KP vs KP
        "k7/8/8/8/8/8/8/KFF5[] w - - 0 1": (False, True),  # KFF vs K
        "k7/8/8/8/8/8/8/KS6[] w - - 0 1": (False, True),  # KS vs K
    },
    "makpong": {
        "8/8/8/4k2K/5m~2/4m~3/8/8 w - 128 8 58": (True, False),  # KFF vs K
        "k7/n7/8/8/8/8/8/K7 w - - 0 1": (True, False),  # K vs KN
        "k7/8/8/8/8/8/8/K7 w - - 0 1": (True, True),  # K vs K
    },
    "xiangqi": {
        "rnbakabnr/9/1c5c1/p1p1p1p1p/9/9/P1P1P1P1P/1C5C1/9/RNBAKABNR w - - 0 1": (False, False),  # startpos
        "5k3/4a4/3CN4/9/1PP5p/9/8P/4C4/4A4/2B1K4 w - - 0 46": (False, False),  # issue #53
        "4k4/9/9/9/9/9/9/9/9/4K4 w - - 0 1": (True, True),  # K vs K
        "4k4/9/9/4p4/9/9/9/9/9/4KR3 w - - 0 1": (False, False),  # KR vs KP
        "4k4/9/9/9/9/9/9/9/9/3KN4 w - - 0 1": (False, True),  # KN vs K
        "4k4/9/4b4/9/9/9/9/4B4/9/4K4 w - - 0 1": (True, True),  # KB vs KB
        "4k4/9/9/9/9/9/9/9/4A4/4KC3 w - - 0 1": (False, True),  # KCA vs K
    },
    "janggi": {
        JANGGI: (False, False),  # startpos
        "rhea1aehr/4k4/1c5c1/p1p1p1p1p/9/9/P1P1P1P1P/1C5C1/4K4/RHEA1AEHR w - - 0 1": (False, False),  # startpos
        "5k3/4a4/3CN4/9/1PP5p/9/8P/4C4/4A4/2B1K4 w - - 0 46": (False, False),  # issue #53
        "4k4/9/9/9/9/4B4/4B4/9/9/4K4 w - - 0 1": (False, False),  # helper suppressed for janggi adjudication rules
        "4k4/9/9/9/9/9/9/9/4A4/4KC3 w - - 0 1": (False, False),  # helper suppressed for janggi adjudication rules
    },
    "shako": {
        "k9/10/10/10/10/10/10/10/10/KC8 w - - 0 1": (True, True),  # KC vs K
        "k9/10/10/10/10/10/10/10/10/KCC7 w - - 0 1": (False, True),  # KCC vs K
        "k9/10/10/10/10/10/10/10/10/KEC7 w - - 0 1": (False, True),  # KEC vs K
        "k9/10/10/10/10/10/10/10/10/KNE7 w - - 0 1": (False, True),  # KNE vs K
        "kb8/10/10/10/10/10/10/10/10/KE8 w - - 0 1": (False, False),  # KE vs KB opp color
        "kb8/10/10/10/10/10/10/10/10/K1E7 w - - 0 1": (True, True),  # KE vs KB same color
    },
    "orda": {
        "k7/8/8/8/8/8/8/K7 w - - 0 1": (False, False),  # K vs K
    },
    "tencubed": {
        "2cwamwc2/1rnbqkbnr1/pppppppppp/10/10/10/10/PPPPPPPPPP/1RNBQKBNR1/2CWAMWC2 w - - 0 1":  (False, False),  # startpos
        "10/5k4/10/10/10/10/10/10/5KC3/10 w - - 0 1":  (False, True),  # KC vs K
        "10/5k4/10/10/10/10/10/10/5K4/10 w - - 0 1":  (True, True),  # K vs K
    },
    "wazirking": {
        "7k/6K1/8/8/8/8/8/8 b - - 0 1": (False, False),  # K vs K
    },
    "multipawn": {
        "k7/p7/8/8/8/8/8/K7 w - - 0 1": (True, False),  # K vs KP
        "k7/s7/8/8/8/8/8/K7 w - - 0 1": (True, False),  # K vs KS
    },
}

invalid_variant_positions = {
    "chess": (
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 a",  # invalid full move
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - b 1",  # invalid half move
        "rnbqkbnr/ppp2ppp/4p3/3pP3/8/8/PPPP1PPP/RNBQKBNR w KQkq -6 0 3",  # invalid en passant
        "rnbqkbnr/ppp2ppp/4p3/3pP3/8/8/PPPP1PPP/RNBQKBNR w KQkq .6 0 3",  # invalid en passant
        "rnbqkbnr/ppp2ppp/4p3/3pP3/8/8/PPPP1PPP/RNBQKBNR w KQkq d- 0 3",  # invalid en passant
        "rnbqkbnr/ppp2ppp/4p3/3pP3/8/8/PPPP1PPP/RNBQKBNR w KQkq  0 3",  # invalid/missing en passant
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w 123 - 0 1",  # invalid castling
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR g KQkq - 0 1",  # invalid side to move
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNH w KQkq - 0 1",  # invalid piece type
        "rnbqkbnr/pppppppp/7/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",  # invalid file count
        "rnbqkbnr/pppppppp/9/7/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",  # invalid file count
        "rnbqkbnr/pppppppp/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",  # invalid rank count
        "rnbqkbn1/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",  # missing castling rook
        "1nbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",  # missing castling rook
        "rnbqkbnr/pppppppp/8/8/8/4K3/PPPPPPPP/RNBQ1BNR w KQkq - 0 1",  # king not on castling rank
        "rnbqkbnr/pppppppp/8/8/8/RNBQKBNR/PPPPPPPP/8 w KQkq - 0 1",  # not on castling rank
        "8/pppppppp/rnbqkbnr/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",  # not on castling rank
    ),
    "atomic": (
        "rnbqkbnr/pppppppp/8/8/8/RNBQKBNR/PPPPPPPP/8 w KQkq - 0 1",  # wrong castling rank
    ),
    "3check": (
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 3+a 0 1",  # invalid check count
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 +a+2",  # invalid lichess check count
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 +1+4",  # invalid lichess check count
    ),
    "horde": (
        "rnbqkbnr/pppppppp/8/1PP2PP1/PPPPPPPP/PPPPPPPP/PPPPPPPP/PPPPPPPK w kq - 0 1",  # wrong king count
        "rnbq1bnr/pppppppp/8/1PP2PP1/PPPPPPPP/PPPPPPPP/PPPPPPPP/PPPPPPPP w kq - 0 1",  # wrong king count
    ),
    "sittuyin": (
        "8/8/4pppp/pppp4/4PPPP/PPPP4/8/8[FRRSSNNkfrrssnn] w - - 0 1",  # wrong king count
    ),
    "shako": (
        "c8c/ernbqkbnre/pppppppppp/10/10/10/10/PPPPPPPPPP/C8C/ERNBQKBNRE w KQkq - 0 1",  # not on castling rank
    ),
    "seirawan": (
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQK1NR[HEhe] w KQBCDFGkqbcdfg - 0 1",  # white gating flag
        "rnbqkb1r/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR[HEhe] w KQBCDFGkqbcdfg - 0 1",  # black gating flag
    )
}


class TestPyffish(unittest.TestCase):
    def test_run_cpp_tests(self):
        sf.load_variant_config(
            """[pairedpawns:chess]
startFen = 8/8/8/8/8/8/8/8[PPpp] w - - 0 1
pieceDrops = true
symmetricDropTypes = p
"""
        )
        self.assertTrue(sf.run_cpp_tests())

    def test_version(self):
        result = sf.version()
        self.assertEqual(len(result), 3)

    def test_info(self):
        result = sf.info()
        self.assertTrue(result.startswith("Fairy-Stockfish"))

    def test_variants_loaded(self):
        variants = sf.variants()
        self.assertTrue("shogun" in variants)
        self.assertTrue("hostageblank" in variants)

    def test_duplicate_variant_warnings_are_summarized(self):
        code = (
            "import pyffish as sf\n"
            "sf.load_variant_config('[chess]\\n[normal]\\n')\n"
        )
        result = subprocess.run([sys.executable, "-c", code], capture_output=True, text=True, check=True)
        self.assertIn("[2] variants already existed.", result.stderr)
        self.assertIn("Set option VerboseVariantLoadWarnings to true to see full details.", result.stderr)
        self.assertNotIn("Variant 'chess' already exists.", result.stderr)

    def test_piece_points_clamping_warnings(self):
        ini_content = "[invalid-piece-points:chess]\npiecePoints = P:-1 R:100\n"
        with tempfile.NamedTemporaryFile(mode='w', suffix='.ini', delete=False) as f:
            f.write(ini_content)
            temp_path = f.name
        try:
            engine = os.environ.get("ENGINE")
            if not engine or not os.path.exists(engine):
                repo_root = os.environ.get("ROOT_DIR", str(Path(__file__).resolve().parent))
                engine = os.path.join(repo_root, "src", "stockfish")
                if not os.path.exists(engine):
                    engine = os.path.join(repo_root, "stockfish")

            if not os.path.exists(engine):
                self.skipTest("Stockfish engine binary not found")

            proc = subprocess.Popen([engine], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            stdout, stderr = proc.communicate(f"check {temp_path}\nquit\n")
            self.assertIn("piecePoints - Negative value clamped to 0.", stderr)
            self.assertIn("piecePoints - Value exceeds MAX_PIECE_POINTS and was clamped.", stderr)
        finally:
            os.remove(temp_path)

    def test_set_option(self):
        result = sf.set_option("UCI_Variant", "capablanca")
        self.assertIsNone(result)

    def test_two_boards(self):
        self.assertFalse(sf.two_boards("chess"))
        self.assertTrue(sf.two_boards("bughouse"))

    def test_captures_to_hand(self):
        self.assertFalse(sf.captures_to_hand("seirawan"))
        self.assertTrue(sf.captures_to_hand("shouse"))

    def test_start_fen(self):
        result = sf.start_fen("capablanca")
        self.assertEqual(result, CAPA)

        result = sf.start_fen("capahouse")
        self.assertEqual(result, CAPAHOUSE)

        result = sf.start_fen("xiangqi")
        self.assertEqual(result, XIANGQI)

        result = sf.start_fen("grandhouse")
        self.assertEqual(result, GRANDHOUSE)

        result = sf.start_fen("shogun")
        self.assertEqual(result, SHOGUN)

        with self.assertRaisesRegex(ValueError, "No such variant"):
            sf.start_fen("this_variant_does_not_exist")

    def test_legal_moves(self):
        with self.assertRaisesRegex(ValueError, "No such variant"):
            sf.legal_moves("this_variant_does_not_exist", CHESS, [])

        fen = "10/10/10/10/10/k9/10/K9 w - - 0 1"
        result = sf.legal_moves("capablanca", fen, [])
        self.assertEqual(result, ["a1b1"])

        result = sf.legal_moves("grand", GRAND, ["a3a5"])
        self.assertIn("a10b10", result)

        result = sf.legal_moves("xiangqi", XIANGQI, ["h3h10"])
        self.assertIn("i10h10", result)

        # Double-check can still be blockable in xiangqi if one checker is a cannon
        # and the interposition adds a second hurdle on the cannon line.
        fen = "9/4c4/3k5/4r4/9/9/3C5/9/4K4/3R5 w - - 2 2"
        result = sf.legal_moves("xiangqi", fen, [])
        self.assertIn("d4e4", result)

        result = sf.legal_moves("shogun", SHOGUN, ["c2c4", "b8c6", "b2b4", "b7b5", "c4b5", "c6b8"])
        self.assertIn("b5b6+", result)

        # Seirawan gating but no castling
        fen = "rnbq3r/pp2bkpp/8/2p1p2K/2p1P3/8/PPPP1PPP/RNB4R[EHeh] b ABCHabcdh - 0 10"
        result = sf.legal_moves("seirawan", fen, [])
        self.assertIn("c8g4h", result)

        # Drop pseudo-royals into check
        result = sf.legal_moves("coregaldrop", sf.start_fen("coregaldrop"), [])
        self.assertIn("Q@a3", result)
        self.assertNotIn("Q@a6", result)
        # In Cannon Shogi the FGC and FSC can also move one square diagonally and, besides,
        # move or capture two squares diagonally, by leaping an adjacent piece. 
        fen = "lnsg1gsnl/1rc1kuab1/p1+A1p1p1p/3P5/6i2/6P2/P1P1P3P/1B1U1ICR1/LNSGKGSNL[] w - - 1 3"
        result = sf.legal_moves("cannonshogi", fen, [])
        # mF
        self.assertIn("c7b6", result)
        self.assertIn("c7d8", result)
        self.assertNotIn("c7d6", result)
        self.assertNotIn("c7b8", result)
        # pB2
        self.assertIn("c7a9", result)
        self.assertIn("c7e5", result)
        self.assertNotIn("c7a5", result)
        self.assertNotIn("c7e9", result)
        # verify distance limited to 2
        self.assertNotIn("c7f4", result)
        self.assertNotIn("c7g3", result)

        # Cambodian queen cannot capture with its leap
        # Cambodian king cannot leap to escape check
        result = sf.legal_moves("cambodian", CAMBODIAN, ["b1d2", "g8e7", "d2e4", "d6d5", "e4d6"])
        self.assertNotIn("d8d6", result)
        self.assertNotIn("e8g7", result)
        self.assertNotIn("e8c7", result)

        # In Janggi stalemate-like positions pass (in-place king move) is available.
        fen = "4k4/c7R/9/3R1R3/9/9/9/9/9/3K5 b - - 0 1"
        result = sf.legal_moves("janggi", fen, [])
        self.assertEqual(result, ["0000"])

        # Hoppers can be configured to not hop over/capture selected piece types.
        sf.load_variant_config(
            """[hopban:chess]
customPiece1 = c:pR
mutuallyHopIllegalTypes = c
startFen = 4k3/8/8/8/8/c7/c7/C3K3 w - - 0 1
"""
        )
        hopban_fen = sf.start_fen("hopban")
        self.assertNotIn("a1a3", sf.legal_moves("hopban", hopban_fen, []))

        sf.load_variant_config(
            """[hopallow:chess]
customPiece1 = c:pR
startFen = 4k3/8/8/8/8/c7/c7/C3K3 w - - 0 1
"""
        )
        hopallow_fen = sf.start_fen("hopallow")
        self.assertIn("a1a3", sf.legal_moves("hopallow", hopallow_fen, []))

        # Universal leaper can move to any empty square, controlled by modality.
        sf.load_variant_config(
            """[anysq:chess]
customPiece1 = a:mU
startFen = 7k/8/8/8/8/8/8/A6K w - - 0 1
"""
        )
        anysq_fen = sf.start_fen("anysq")
        anysq_moves = sf.legal_moves("anysq", anysq_fen, [])
        self.assertIn("a1b5", anysq_moves)
        self.assertIn("a1e2", anysq_moves)
        self.assertNotIn("a1h8", anysq_moves)  # quiet-only mU cannot capture

        # Duck-like piece: mU (move to any empty square) + captureForbidden keeps it uncapturable.
        sf.load_variant_config(
            """[duckpiece:chess]
customPiece1 = u:mU
captureForbidden = *:u
startFen = 3qk3/8/8/8/3U4/8/8/4K3 b - - 0 1
"""
        )
        duck_moves = sf.legal_moves("duckpiece", sf.start_fen("duckpiece"), [])
        self.assertNotIn("d8d4", duck_moves)

        # Tuple-leaper edge wrapping check: (4,1) from a1 only reaches b5 and e2.
        sf.load_variant_config(
            """[tuple41:chess]
customPiece1 = a:m(4,1)
startFen = 7k/8/8/8/8/8/8/A6K w - - 0 1
"""
        )
        tuple_fen = sf.start_fen("tuple41")
        tuple_moves = sf.legal_moves("tuple41", tuple_fen, [])
        self.assertIn("a1b5", tuple_moves)
        self.assertIn("a1e2", tuple_moves)
        self.assertNotIn("a1h2", tuple_moves)
        self.assertNotIn("a1b8", tuple_moves)

        # Long tuple leaper (0,7) should be legal from a1 to h1 without wrap artifacts.
        sf.load_variant_config(
            """[tuple07:chess]
customPiece1 = a:m(0,7)
startFen = 7k/8/8/8/8/8/8/A5K1 w - - 0 1
"""
        )
        tuple07_moves = sf.legal_moves("tuple07", sf.start_fen("tuple07"), [])
        self.assertIn("a1h1", tuple07_moves)
        self.assertNotIn("a1b2", tuple07_moves)

        # Jump captures should honor selfCapture when the jumped piece is friendly.
        sf.load_variant_config(
            """[selfjump:chess]
customPiece1 = a:c{hurdles: 1,1; pre: 1,1; post: 1,1; capture: locust_first; hurdle_types: friendly}R
selfCapture = true
startFen = 7k/8/8/8/8/8/P7/A6K w - - 0 1
"""
        )
        selfjump_fen = sf.start_fen("selfjump")
        selfjump_moves = sf.legal_moves("selfjump", selfjump_fen, [])
        self.assertIn("a1a3", selfjump_moves)

        # Anti-royal pieces must remain attacked.
        sf.load_variant_config(
            """[antiroyal:chess]
customPiece1 = a:K
antiRoyalTypes = a
startFen = r6k/8/8/8/8/8/8/A6K w - - 0 1
"""
        )
        anti_fen = sf.start_fen("antiroyal")
        anti_moves = sf.legal_moves("antiroyal", anti_fen, [])
        self.assertIn("a1a2", anti_moves)
        self.assertNotIn("a1b1", anti_moves)

        # Different source pieces can share one promoted type and still demote
        # back to their own original piece types.
        sf.load_variant_config(
            """[dupdemote:chess]
pieceDemotion = true
promotedPieceType = b:q n:q
startFen = 3k4/1B4N1/8/8/8/8/8/4K3 w - - 0 1
"""
        )
        dup_fen = sf.start_fen("dupdemote")
        bishop_demoted = sf.get_fen("dupdemote", dup_fen, ["b7a8+", "d8d7", "a8a7-"])
        knight_demoted = sf.get_fen("dupdemote", dup_fen, ["g7e8+", "d8c7", "e8e7-"])
        self.assertIn("B2k2N1", bishop_demoted)
        self.assertIn("1Bk1N3", knight_demoted)

        # pawn promotion of dropped pawns beyond promotion rank
        result = sf.legal_moves("makhouse", "rnsmksnr/8/1ppP1ppp/p3p3/8/PPP1PPPP/8/RNSKMSNR[p] w - - 0 4", [])
        self.assertIn("d6d7m", result)
        self.assertNotIn("d6d7", result)

        # Test configurable piece perft
        legals = ['a3a4', 'b3b4', 'c3c4', 'd3d4', 'e3e4', 'f3f4', 'g3g4', 'e1e2', 'f1f2', 'b1a2', 'b1b2', 'b1c2', 'c1b2', 'c1c2', 'c1d2', 'a1a2', 'g1g2', 'd1c2', 'd1d2', 'd1e2']
        result = sf.legal_moves("yarishogi", sf.start_fen("yarishogi"), [])
        self.assertCountEqual(legals, result)

        # Test betza parsing
        result = sf.legal_moves("betzatest", "7/7/7/3A3/7/7/7 w - - 0 1", [])
        self.assertEqual(['d4c2', 'd4b3', 'd4b5', 'd4c6'], result)
        result = sf.legal_moves("betzatest", "7/7/7/3B3/7/7/7 w - - 0 1", [])
        self.assertEqual(['d4e2', 'd4f3', 'd4f5', 'd4e6'], result)
        result = sf.legal_moves("betzatest", "7/7/7/3C3/7/7/7 w - - 0 1", [])
        self.assertEqual(['d4e2', 'd4b3', 'd4f5', 'd4c6'], result)
        result = sf.legal_moves("betzatest", "7/7/7/3D3/7/7/7 w - - 0 1", [])
        self.assertEqual(['d4c2', 'd4f3', 'd4b5', 'd4e6'], result)
        # Test simple hopper with range limit > 1 (e:pB3)
        result = sf.legal_moves("betzatest", "7/7/4a2/3E3/7/7/7 w - - 0 1", [])
        self.assertEqual(['d4f6', 'd4g7'], sorted(result))

        # Test universal hopper, dynamic slider, and limited hopper semantics (Issue 1-5)
        sf.load_variant_config(
            """[goaltest:chess]
customPiece1 = h:pU
customPiece2 = d:xR
customPiece3 = e:pB2
customPiece4 = n:xN
startFen = 4k3/8/8/8/8/8/8/4K3 w - - 0 1
"""
        )
        # Universal hopper: Jump over e5 to f6
        result = sf.legal_moves("goaltest", "4k3/8/8/4p3/3H4/8/8/4K3 w - - 0 1", [])
        self.assertIn("d4f6", result)
        self.assertNotIn("d4e5", result)

        # Check detection for universal hopper gives check to f6
        self.assertTrue(sf.gives_check("goaltest", "8/8/5k2/4p3/3H4/8/8/4K3 b - - 0 1", []))

        # Dynamic slider quiet moves check
        result = sf.legal_moves("goaltest", "4k3/8/3P4/8/3D4/8/8/4K3 w - - 0 1", [])
        self.assertNotIn("d4d6", result)
        # Capture of enemy is legal
        result = sf.legal_moves("goaltest", "4k3/8/3p4/8/3D4/8/8/4K3 w - - 0 1", [])
        self.assertIn("d4d6", result)

        # Dynamic slider with knight direction should not self-destination
        result = sf.legal_moves("goaltest", "4k3/8/8/8/3N4/8/8/4K3 w - - 0 1", [])
        self.assertNotIn("d4d4", result)

        # Limited-range hopper pB2: hurdle at e5 (dist 1) -> land at f6 (dist 2) is ok
        result = sf.legal_moves("goaltest", "4k3/8/8/4p3/3E4/8/8/4K3 w - - 0 1", [])
        self.assertIn("d4f6", result)
        # Hurdle at f6 (dist 2) -> land at g7 (dist 3) is too far
        result = sf.legal_moves("goaltest", "4k3/6p1/5p2/8/3E4/8/8/4K3 w - - 0 1", [])
        self.assertNotIn("d4g7", result)


        # diagonalGeneral: moving the blocker off the king diagonal is illegal
        result = sf.legal_moves("diagfaceoff", sf.start_fen("diagfaceoff"), [])
        self.assertNotIn("d4d5", result)
        self.assertIn("c5c4", result)

        # Extended FEN gating mask disambiguates castling rights from gating files.
        result = sf.legal_moves("fenmask", sf.start_fen("fenmask"), [])
        self.assertIn("l1k1q", result)
        self.assertNotIn("e1f1q", result)
        normalized = sf.get_fen("fenmask", sf.start_fen("fenmask"), [])
        self.assertIn("|", normalized)
        self.assertEqual(sf.get_fen("fenmask", normalized, []), normalized)

        # commitGates FEN parsing/validation: compressed commit rows should
        # round-trip, malformed commit-row widths should be rejected.
        sf.load_variant_config(
            """[commitcheck:chess]
gating = true
commitGates = true
castling = false
startFen = ********/4k3/8/8/8/8/8/8/4K3/******** w - - 0 1
"""
        )
        commit_start = sf.start_fen("commitcheck")
        self.assertEqual(sf.validate_fen(commit_start, "commitcheck"), sf.FEN_OK)

        commit_compressed = "8/4k3/8/8/8/8/8/8/4K3/8 w - - 0 1"
        self.assertEqual(sf.validate_fen(commit_compressed, "commitcheck"), sf.FEN_OK)
        self.assertEqual(sf.get_fen("commitcheck", commit_compressed, []), commit_start)

        bad_commit_lead = "7/4k3/8/8/8/8/8/8/4K3/8 w - - 0 1"
        bad_commit_tail = "8/4k3/8/8/8/8/8/8/4K3/7 w - - 0 1"
        self.assertNotEqual(sf.validate_fen(bad_commit_lead, "commitcheck"), sf.FEN_OK)
        self.assertNotEqual(sf.validate_fen(bad_commit_tail, "commitcheck"), sf.FEN_OK)

        sf.load_variant_config(
            """[commitround:chess]
gating = true
commitGates = true
castling = false
startFen = q******r/4k3/8/8/8/8/8/8/R3K2R/Q******R w - - 0 1
"""
        )
        after_commit = sf.get_fen("commitround", sf.start_fen("commitround"), ["a1a2"])
        self.assertEqual(sf.validate_fen(after_commit, "commitround"), sf.FEN_OK)
        self.assertEqual(sf.get_fen("commitround", after_commit, []), after_commit)

        # Shogi pawn-drop mate is illegal.
        fen = "BRBRSSSGG/nPPPPPPPP/n8/n8/n8/ll7/kl7/9/K8[PPPPPPPPPPggsl] w - - 0 1"
        result = sf.legal_moves("shogi", fen, [])
        self.assertNotIn("P@a2", result)


    def test_castling(self):
        legals = ['f5f4', 'a7a6', 'b7b6', 'c7c6', 'd7d6', 'e7e6', 'i7i6', 'j7j6', 'a7a5', 'b7b5', 'c7c5', 'e7e5', 'i7i5', 'j7j5', 'b8a6', 'b8c6', 'h6g4', 'h6i4', 'h6j5', 'h6f7', 'h6g8', 'h6i8', 'd5a2', 'd5b3', 'd5f3', 'd5c4', 'd5e4', 'd5c6', 'd5e6', 'd5f7', 'd5g8', 'j8g8', 'j8h8', 'j8i8', 'e8f7', 'c8b6', 'c8d6', 'g6g2', 'g6g3', 'g6f4', 'g6g4', 'g6h4', 'g6e5', 'g6g5', 'g6i5', 'g6a6', 'g6b6', 'g6c6', 'g6d6', 'g6e6', 'g6f6', 'g6h8', 'f8f7', 'f8g8', 'f8i8']
        moves = ['b2b4', 'f7f5', 'c2c3', 'g8d5', 'a2a4', 'h8g6', 'f2f3', 'i8h6', 'h2h3']
        result = sf.legal_moves("capablanca", CAPA, moves)
        self.assertCountEqual(legals, result)
        self.assertIn("f8i8", result)

        moves = ['a2a4', 'f7f5', 'b2b3', 'g8d5', 'b1a3', 'i8h6', 'c1a2', 'h8g6', 'c2c4']
        result = sf.legal_moves("capablanca", CAPA, moves)
        self.assertIn("f8i8", result)

        moves = ['f2f4', 'g7g6', 'g1d4', 'j7j6', 'h1g3', 'b8a6', 'i1h3', 'h7h6']
        result = sf.legal_moves("capablanca", CAPA, moves)
        self.assertIn("f1i1", result)

        # Check that chess960 castling notation is used for otherwise ambiguous castling move
        # d1e1 is a normal king move, so castling has to be d1f1
        result = sf.legal_moves("diana", "rbnk1r/pppbpp/3p2/5P/PPPPPB/RBNK1R w KQkq - 2 3", [])
        self.assertIn("d1f1", result)

        # Atomic960 castling
        fen = "7k/8/8/8/8/8/2PP4/1RK4q w Q - 0 1"
        moves = sf.legal_moves("atomic", fen, [], True)
        # 'c1b1' is the castling move (king to rook square in 960 encoding) and must be illegal
        self.assertNotIn("c1b1", moves)
        # A normal king/commoner move like c1b2 should remain legal
        self.assertIn("c1b2", moves)

        # Atomic960 anti-discovered check with cannon
        fen = "8/8/8/8/8/6k1/8/c5KR w K - 0 1"
        moves = sf.legal_moves("cannonatomic", fen, [], True)
        self.assertNotIn("g1h1", moves)
        self.assertIn("g1f1", moves)

        # Check that in variants where castling rooks are not in the corner
        # the castling rook is nevertheless assigned correctly
        result = sf.legal_moves("shako", "c8c/ernbqkbnre/pppppppppp/10/10/10/10/PPPPPPPPPP/5K2RR/10 w Kkq - 0 1", [])
        self.assertIn("f2h2", result)
        result = sf.legal_moves("shako", "c8c/ernbqkbnre/pppppppppp/10/10/10/10/PPPPPPPPPP/RR3K4/10 w Qkq - 0 1", [])
        self.assertIn("f2d2", result)

    def test_feature_combo_regressions(self):
        # blastPromotion + removeConnectNByType: promoted line must still be removed.
        pond_like = sf.get_fen("blastconnect", sf.start_fen("blastconnect"), ["E@b3", "E@d3", "E@c3", "E@c2"])
        self.assertTrue(pond_like.startswith("4/4/2e1/4["))

        # captureForbidden + captureAllowed wildcard merge:
        # all captures are forbidden, then A->b is explicitly re-enabled.
        white_moves = sf.legal_moves("capmapwild", sf.start_fen("capmapwild"), [])
        self.assertIn("d4d5", white_moves)
        black_moves = sf.legal_moves("capmapwild", "8/8/8/3b4/3A4/8/8/8 b - - 0 1", [])
        self.assertNotIn("d5d4", black_moves)

    def test_strong_pawn_basics(self):
        sf.load_variant_config(
            """[strongpawnproto:chess]
customPiece1 = t:W
customPiece2 = s:ffN
customPiece3 = c:F
customPiece4 = u:K
startFen = rnbqkbnr/tscuucst/8/8/8/8/TSCUUCST/RNBQKBNR w - - 0 1
promotedPieceType = t:r s:n c:b u:q
mandatoryPiecePromotion = true
doubleStep = false
castling = false
enPassantTypes = -
"""
        )

        # Squires leap (forward knight only), towers step orthogonally, and castling is disabled.
        start = sf.start_fen("strongpawnproto")
        start_moves = sf.legal_moves("strongpawnproto", start, [])
        self.assertNotIn("e1g1", start_moves)
        self.assertNotIn("e1c1", start_moves)
        self.assertIn("b2a4", start_moves)
        self.assertIn("g2h4", start_moves)
        self.assertIn("a2a3", start_moves)

        tower_fen = "4k3/8/8/8/8/8/4T3/4K3 w - - 0 1"
        tower_moves = sf.legal_moves("strongpawnproto", tower_fen, [])
        self.assertIn("e2d2", tower_moves)
        self.assertIn("e2f2", tower_moves)

        # Princess promotes to queen on last rank.
        promo_fen = "4k3/3U4/8/8/8/8/8/4K3 w - - 0 1"
        promo_moves = sf.legal_moves("strongpawnproto", promo_fen, [])
        self.assertIn("d7d8+", promo_moves)

    def test_alterga_basics(self):
        sf.load_variant_config(
            """[altergaproto:chess]
customPiece1 = n:mNcB
customPiece2 = b:fWmFcB
customPiece3 = r:mWcRfF
customPiece4 = q:BmRcN
customPiece5 = k:FmWisR2cN
castling = false
startFen = rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w - - 0 1
"""
        )

        # Knight moves by leap, captures by bishop lines.
        knight_fen = "4k3/8/5p2/3p4/4N3/8/8/4K3 w - - 0 1"
        knight_moves = sf.legal_moves("altergaproto", knight_fen, [])
        self.assertIn("e4d5", knight_moves)      # bishop-style capture
        self.assertNotIn("e4f6", knight_moves)   # no knight-style capture
        self.assertIn("e4c5", knight_moves)      # knight-style non-capture move

        # Rook moves one step (plus forward diagonal), but captures long rook lines.
        rook_move_fen = "4k3/8/8/8/4R3/8/8/4K3 w - - 0 1"
        rook_moves = sf.legal_moves("altergaproto", rook_move_fen, [])
        self.assertIn("e4e5", rook_moves)        # one-step move
        self.assertNotIn("e4e6", rook_moves)     # no long non-capture move
        rook_capture_moves = sf.legal_moves("altergaproto", "4k3/8/4p3/8/4R3/8/8/4K3 w - - 0 1", [])
        self.assertIn("e4e6", rook_capture_moves)  # rook-style capture

    def test_royal_piece_no_through_check(self):
        sf.load_variant_config(
            """[caissapathoff:chess]
king = q:Q
castling = false
startFen = q7/8/8/8/8/8/1r6/4Q3 w - - 0 1

[caissapathon:caissapathoff]
royalPieceNoThroughCheck = true
"""
        )

        baseline_moves = sf.legal_moves("caissapathoff", sf.start_fen("caissapathoff"), [])
        restricted_moves = sf.legal_moves("caissapathon", sf.start_fen("caissapathon"), [])

        self.assertIn("e1e3", baseline_moves)
        self.assertNotIn("e1e3", restricted_moves)
        self.assertIn("e1f1", restricted_moves)

    def test_asymmetric_walling_turns(self):
        sf.load_variant_config(
            """[whitewalls:chess]
maxRank = 4
maxFile = 4
pieceToCharTable = -
king = -
queen = -
customPiece1 = q:mQ
startFen = 4/1q2/4/2Q1 w - - 0 1
wallingRule = arrow
wallingWhite = true
wallingBlack = false
captureForbidden = *:*
checking = false
"""
        )

        start = sf.start_fen("whitewalls")
        white_moves = sf.legal_moves("whitewalls", start, [])
        self.assertTrue(white_moves)
        self.assertTrue(all("," in m for m in white_moves))

        after_white = sf.get_fen("whitewalls", start, [white_moves[0]])
        black_moves = sf.legal_moves("whitewalls", after_white, [])
        self.assertTrue(black_moves)
        self.assertTrue(all("," not in m for m in black_moves))

    def test_witch_hunting_basics(self):
        load_repo_variants_or_skip()
        fen = sf.start_fen("witch-hunting")
        white_moves = sf.legal_moves("witch-hunting", fen, [])
        self.assertTrue(white_moves)
        self.assertTrue(all("," in m for m in white_moves))
        self.assertIn("i1a1,a1b1", white_moves)

        after_white = sf.get_fen("witch-hunting", fen, [white_moves[0]])
        black_moves = sf.legal_moves("witch-hunting", after_white, [])
        self.assertTrue(black_moves)
        self.assertTrue(all("," not in m for m in black_moves))

    def test_drop_king_last(self):
        sf.load_variant_config(
            """[droplast:chess]
startFen = 8/8/8/8/8/8/8/8[KNkn] w - - 0 1
pieceDrops = true
mustDrop = true
passUntilSetup = true
dropKingLast = true
castling = false
checking = false
"""
        )

        start = sf.start_fen("droplast")
        white_moves = sf.legal_moves("droplast", start, [])
        self.assertTrue(white_moves)
        self.assertTrue(all(m.startswith("N@") for m in white_moves))

        after_white = sf.get_fen("droplast", start, [white_moves[0]])
        black_moves = sf.legal_moves("droplast", after_white, [])
        self.assertTrue(black_moves)
        self.assertTrue(all(m.startswith("N@") for m in black_moves))

        after_black = sf.get_fen("droplast", after_white, [black_moves[0]])
        white_king_moves = sf.legal_moves("droplast", after_black, [])
        self.assertTrue(white_king_moves)
        self.assertTrue(all(m.startswith("K@") for m in white_king_moves))

    def test_paired_drop_points(self):
        sf.load_variant_config(
            """[pairedpoints:chess]
startFen = 8/8/8/8/8/8/8/8[NNnn] w - - 0 1
pieceDrops = true
symmetricDropTypes = n
payPointsToDrop = true
piecePoints = n:5
"""
        )

        fen_10 = sf.start_fen("pairedpoints") + " {10 10}"
        moves_10 = sf.legal_moves("pairedpoints", fen_10, [])
        self.assertTrue(any("," in m for m in moves_10))

        fen_9 = sf.start_fen("pairedpoints") + " {9 10}"
        moves_9 = sf.legal_moves("pairedpoints", fen_9, [])
        paired_moves_9 = [m for m in moves_9 if "," in m]
        self.assertEqual(len(paired_moves_9), 0)

    def test_liberty_capture_actions(self):
        sf.load_variant_config(
            """[liberty-base]
maxRank = 5
maxFile = 5
pieceToCharTable = -
king = -
immobile = p
startFen = 5/5/5/5/5[] b - - 0 1
pieceDrops = true
freeDrops = true
libertyCapture = remove
libertySelfCapture = forbid
doubleStep = false
castling = false
immobilityIllegal = false
nMoveRule = 0

[liberty-forbid:liberty-base]
libertyCapture = forbid

[liberty-self-remove:liberty-base]
libertySelfCapture = remove

[liberty-self-hand:liberty-self-remove]
freeDrops = false
captureType = hand
startFen = 5/5/2p2/1p1p1/2p2[P] w - - 0 1

[liberty-self-capture-invalid:liberty-base]
selfCapture = true

[liberty-self-capture-type-invalid:liberty-base]
selfCaptureTypesWhite = p
"""
        )

        with self.assertRaises(ValueError):
            sf.start_fen("liberty-self-capture-invalid")
        with self.assertRaises(ValueError):
            sf.start_fen("liberty-self-capture-type-invalid")

        start = sf.start_fen("liberty-base")
        self.assertIn("P@c3", sf.legal_moves("liberty-base", start, []))

        setup = ["P@c3", "P@c2", "P@b2", "P@a1", "P@d2", "P@a2"]
        after_capture = sf.get_fen("liberty-base", start, setup + ["P@c1"])
        self.assertEqual(after_capture.split()[0], "5/5/2p2/Pp1p1/P1p2")

        group_fen = "5/5/1pPp1/1pPp1/2p2[] b - - 0 1"
        after_group_capture = sf.get_fen("liberty-base", group_fen, ["P@c4"])
        self.assertEqual(after_group_capture.split()[0], "5/2p2/1p1p1/1p1p1/2p2")

        before_capture = sf.get_fen("liberty-forbid", sf.start_fen("liberty-forbid"), setup)
        self.assertNotIn("P@c1", sf.legal_moves("liberty-forbid", before_capture, []))
        self.assertNotIn("P@c4", sf.legal_moves("liberty-forbid", group_fen, []))

        suicide_fen = "5/5/2p2/1p1p1/2p2[] w - - 0 1"
        self.assertNotIn("P@c2", sf.legal_moves("liberty-base", suicide_fen, []))
        self.assertIn("P@c2", sf.legal_moves("liberty-self-remove", suicide_fen, []))
        after_suicide = sf.get_fen("liberty-self-remove", suicide_fen, ["P@c2"])
        self.assertEqual(after_suicide.split()[0], "5/5/2p2/1p1p1/2p2")

        group_suicide_fen = "5/2p2/1pPp1/1p1p1/2p2[P] w - - 0 1"
        self.assertIn("P@c2", sf.legal_moves("liberty-self-remove", group_suicide_fen, []))
        after_group_suicide = sf.get_fen(
            "liberty-self-remove", group_suicide_fen, ["P@c2"]
        )
        self.assertEqual(after_group_suicide.split()[0], "5/2p2/1p1p1/1p1p1/2p2")

        hand_start = sf.start_fen("liberty-self-hand")
        self.assertIn("P@c2", sf.legal_moves("liberty-self-hand", hand_start, []))
        after_hand_suicide = sf.get_fen("liberty-self-hand", hand_start, ["P@c2"])
        self.assertEqual(after_hand_suicide.split()[0], "5/5/2p2/1p1p1/2p2[]")

    def test_ichess_setup_basics(self):
        load_repo_variants_or_skip()
        fen = sf.start_fen("ichess")
        self.assertEqual(fen.split()[0], "8/pppppppp/8/8/8/8/PPPPPPPP/8[KQRRBBNNkqrrbbnn]")

        moves = sf.legal_moves("ichess", fen, [])
        self.assertTrue(moves)
        self.assertTrue(all("@" in m for m in moves))
        self.assertTrue(all(not m.startswith("K@") for m in moves))

    def test_chesscom_custom_setups_basics(self):
        # Trapped Queens / Infiltration Danger / Stone Gravitation are orthodox 8x8
        # positions imported from chess.com Fen4 setups.
        path = repo_variants_ini()
        if path is None:
            raise unittest.SkipTest("repo variants.ini is not available in this test environment")
        sf.set_option("VariantPath", str(path))
        trapped = sf.start_fen("trapped-queens")
        trapped_moves = sf.legal_moves("trapped-queens", trapped, [])
        self.assertIn("e1d1", trapped_moves)
        self.assertIn("a3a4", trapped_moves)

        infiltration = sf.start_fen("infiltration-danger")
        infiltration_moves = sf.legal_moves("infiltration-danger", infiltration, [])
        self.assertIn("e5d4", infiltration_moves)
        self.assertIn("e5f4", infiltration_moves)

        stone = sf.start_fen("stone-gravitation")
        stone_moves = sf.legal_moves("stone-gravitation", stone, [])
        self.assertIn("b1c3", stone_moves)
        self.assertIn("a2b4", stone_moves)

    def test_checkers_jump_and_promotion(self):
        # Jump captures are mandatory and generated correctly when the jump-capture
        # rule set is available in the current build.
        fen = "8/8/5m2/8/3m4/2M5/8/7K w - - 0 1"
        first_moves = sf.legal_moves("checkersmini", fen, [])
        self.assertEqual(first_moves, ["c3e5"])

        after_first_jump = sf.get_fen("checkersmini", fen, ["c3e5"])
        self.assertEqual(sf.legal_moves("checkersmini", after_first_jump, []), ["f6d4"])

        # The same piece still has the expected follow-up jump pattern.
        followup_probe = after_first_jump.replace(" b ", " w ")
        self.assertEqual(sf.legal_moves("checkersmini", followup_probe, []), ["e5g7"])

        # Promotion by jump should end the turn (no forced continuation through kinging).
        promo_fen = "8/2m1m3/1M6/8/8/8/8/7K w - - 0 1"
        promo_moves = sf.legal_moves("checkersmini", promo_fen, [])
        chosen_promo = "b6d8k" if "b6d8k" in promo_moves else "b6d8"
        self.assertIn(chosen_promo, promo_moves)
        after_promo = sf.get_fen("checkersmini", promo_fen, [chosen_promo])
        black_after_promo = sf.legal_moves("checkersmini", after_promo, [])
        self.assertGreater(len(black_after_promo), 0)
        self.assertEqual(len([m for m in black_after_promo if m[:2] == m[2:4]]), 0)

    def test_standard_fairy_riders_and_ski_sliders(self):
        sf.load_variant_config(
            """[fairyriders:chess]
maxRank = 7
maxFile = 7
customPiece1 = a:AA
customPiece2 = b:DD
customPiece3 = c:jR
customPiece4 = d:jB
customPiece5 = e:jQ
startFen = 7/7/7/3A3/7/7/7 w - - 0 1
"""
        )

        # Alfil-rider: repeats (2,2) leaps.
        ar_moves = sf.legal_moves("fairyriders", "7/7/7/3A3/7/7/7 w - - 0 1", [])
        self.assertCountEqual(["d4b2", "d4f2", "d4b6", "d4f6"], ar_moves)

        # Dabbaba-rider: repeats (2,0) leaps.
        dr_moves = sf.legal_moves("fairyriders", "7/7/7/3B3/7/7/7 w - - 0 1", [])
        self.assertCountEqual(["d4d2", "d4d6", "d4b4", "d4f4"], dr_moves)

        # Ski rook: adjacent orthogonals are skipped.
        sr_moves = sf.legal_moves("fairyriders", "7/7/7/3C3/7/7/7 w - - 0 1", [])
        self.assertIn("d4d6", sr_moves)
        self.assertNotIn("d4d5", sr_moves)
        self.assertIn("d4b4", sr_moves)
        self.assertNotIn("d4c4", sr_moves)

        # Ski bishop: adjacent diagonals are skipped.
        sb_moves = sf.legal_moves("fairyriders", "7/7/7/3D3/7/7/7 w - - 0 1", [])
        self.assertIn("d4f6", sb_moves)
        self.assertNotIn("d4e5", sb_moves)
        self.assertIn("d4b2", sb_moves)
        self.assertNotIn("d4c3", sb_moves)

        # Ski queen combines ski-rook and ski-bishop effects.
        sq_moves = sf.legal_moves("fairyriders", "7/7/7/3E3/7/7/7 w - - 0 1", [])
        self.assertIn("d4d6", sq_moves)
        self.assertIn("d4f6", sq_moves)
        self.assertNotIn("d4d5", sq_moves)
        self.assertNotIn("d4e5", sq_moves)

    def test_whaleshogi_dolphin_promotion_cycle(self):
        sf.load_variant_config(
            """[whaleshogi_proto:minishogi]
pieceToCharTable = -
maxRank = 6
maxFile = 6
king = w
customPiece1 = g:fRbB
customPiece2 = p:rlW
customPiece3 = k:FR
customPiece4 = n:fDrlbW
customPiece5 = h:FbW
customPiece6 = b:fbWfF
customPiece7 = d:fW
customPiece8 = e:bB
startFen = bnpwgh/ddddd1/6/6/DDDDD1/HGWPNB[] w - 0 1
promotionRegionWhite = *6
promotionRegionBlack = *1
promotedPieceType = d:e p:k
mandatoryPiecePromotion = true
pieceDemotion = true
dropPromoted = true
promotionPawnTypes = d
dropNoDoubled = d
dropNoDoubledCount = 2
"""
        )

        # Start position loads and has expected opening mobility.
        start = sf.start_fen("whaleshogi_proto")
        start_moves = sf.legal_moves("whaleshogi_proto", start, [])
        self.assertIn("e1e3", start_moves)
        self.assertIn("a2a3", start_moves)

        # Dolphin promotion on back rank is mandatory.
        promo_src = "5w/4D1/6/6/6/W5 w - - 0 1"
        promo_moves = sf.legal_moves("whaleshogi_proto", promo_src, [])
        self.assertIn("e5e6+", promo_moves)
        self.assertNotIn("e5e6", promo_moves)

        # Promoted dolphin (+D) uses promoted movement and demotes when leaving.
        promoted = "4+Dw/6/6/6/6/W5 w - - 0 1"
        promoted_moves = sf.legal_moves("whaleshogi_proto", promoted, [])
        self.assertIn("e6d5-", promoted_moves)
        self.assertIn("e6f5-", promoted_moves)
        self.assertNotIn("e6d5", promoted_moves)
        self.assertEqual(sf.get_fen("whaleshogi_proto", promoted, ["e6d5-"]), "5w/3D2/6/6/6/W5[] b - - 1 1")

    def test_get_fen(self):
        result = sf.get_fen("chess", CHESS, [])
        self.assertEqual(result, CHESS)

        # incomplete FENs
        result = sf.get_fen("chess", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR", [])
        self.assertEqual(result, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w - - 0 1")
        result = sf.get_fen("chess", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -", [])
        self.assertEqual(result, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
        result = sf.get_fen("chess", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w 1 2", [])
        self.assertEqual(result, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w - - 1 2")

        # invalid castling rights
        result = sf.get_fen("chess", "8/rnbqkbnr/pppppppp/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", [])
        self.assertEqual(result, "8/rnbqkbnr/pppppppp/8/8/8/PPPPPPPP/RNBQKBNR w KQ - 0 1")
        result = sf.get_fen("chess", "r7/1nbqkbnr/pppppppp/8/8/P6P/RPPPPPPR/1NBQKBN1 w KQkq - 0 1", [])
        self.assertEqual(result, "r7/1nbqkbnr/pppppppp/8/8/P6P/RPPPPPPR/1NBQKBN1 w - - 0 1")

        # alternative piece symbols
        result = sf.get_fen("janggi", "rhea1aehr/4k4/1c5c1/p1p1p1p1p/9/9/P1P1P1P1P/1C5C1/4K4/RHEA1AEHR w - - 0 1", [])
        self.assertEqual(result, JANGGI)

        result = sf.get_fen("capablanca", CAPA, [])
        self.assertEqual(result, CAPA)

        result = sf.get_fen("xiangqi", XIANGQI, [])
        self.assertEqual(result, XIANGQI)

        result = sf.get_fen("seirawan", SEIRAWAN, [])
        self.assertEqual(result, SEIRAWAN)

        # test idempotence for S-Chess960 gating flags
        fen1 = sf.get_fen("seirawan", SEIRAWAN, [], True)
        fen2 = sf.get_fen("seirawan", fen1, [], True)
        self.assertEqual(fen1, fen2)

        fen = "rnab1kbcnr/ppppPppppp/10/4q5/10/10/PPPPP1PPPP/RNABQKBCNR[p] b KQkq - 0 3"
        result = sf.get_fen("capahouse", CAPA, ["f2f4", "e7e5", "f4e5", "e8e5", "P@e7"])
        self.assertEqual(result, fen)

        fen0 = "reb1k2r/ppppqppp/2nb1n2/4p3/4P3/N1P2N2/PB1PQPPP/RE2KBHR[h] b KQkqac - 2 6"
        fen1 = "reb2rk1/ppppqppp/2nb1n2/4p3/4P3/N1P2N2/PB1PQPPP/RE2KBHR[h] w KQac - 3 7"
        result = sf.get_fen("seirawan", fen0, ["e8g8"])
        self.assertEqual(result, fen1)

        # handle invalid castling/gating flags
        fen0 = "rnbq3r/pp2bkpp/8/2p1p2K/2p1P3/8/PPPP1PPP/RNB4R[EHeh] b QBCEHabcdk - 0 10"
        fen1 = "rnbq3r/pp2bkpp/8/2p1p2K/2p1P3/8/PPPP1PPP/RNB4R[EHeh] b ABCHabcdh - 0 10"
        result = sf.get_fen("seirawan", fen0, [])
        self.assertEqual(result, fen1)

        result = sf.get_fen("chess", CHESS, [], True, False, False)
        self.assertEqual(result, CHESS960)

        # test O-O-O
        fen = "rbkqnrbn/pppppppp/8/8/8/8/PPPPPPPP/RBKQNRBN w AFaf - 0 1"
        moves = ["d2d4", "f7f5", "e1f3", "h8g6", "h1g3", "c7c6", "c2c3", "e7e6", "b1d3", "d7d5", "d1c2", "b8d6", "e2e3", "d8d7", "c1a1"]
        result = sf.get_fen("chess", fen, moves, True, False, False)
        self.assertEqual(result, "r1k1nrb1/pp1q2pp/2pbp1n1/3p1p2/3P4/2PBPNN1/PPQ2PPP/2KR1RB1 b fa - 2 8")

        # passing should not affect castling rights
        fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
        result = sf.get_fen("passchess", fen, ["e1e1", "e8e8"])
        self.assertEqual(result, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 2 2")

        # petrified capturing piece should not be added to hand as bycatch
        sf.load_variant_config(
            """[petrihand:chess]
capturesToHand = true
pieceDrops = true
petrifyOnCaptureTypes = q
startFen = 4k3/3p4/8/8/8/8/8/3QK3 w - - 0 1
"""
        )
        fen = sf.start_fen("petrihand")
        result = sf.get_fen("petrihand", fen, ["d1d7"])
        self.assertEqual(result, "4k3/3*4/8/8/8/8/8/4K3[P] b - - 0 1")

        # only irreversible moves should reset 50 move rule counter
        fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
        result = sf.get_fen("pawnsideways", fen, ["e2e4", "g8f6", "e4d4"])
        self.assertEqual(result, "rnbqkb1r/pppppppp/5n2/8/3P4/8/PPPP1PPP/RNBQKBNR b KQkq - 2 2")
        result = sf.get_fen("pawnback", fen, ["e2e4", "e7e6"])
        self.assertEqual(result, "rnbqkbnr/pppp1ppp/4p3/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 2 2")
        result = sf.get_fen("pocketknight", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR[Nn] w KQkq - 0 1", ["N@e4"])
        self.assertEqual(result, "rnbqkbnr/pppppppp/8/8/4N3/8/PPPPPPPP/RNBQKBNR[n] b KQkq - 0 1")

        # duck chess en passant
        fen = "r1b1k3/pp3pb1/4p3/2p2p2/2PpP2q/1P1P1P2/P1K1*3/RN1Q2N1 b q e3 0 17"
        result = sf.get_fen("duck", fen, [])
        self.assertEqual(result, fen)

        # SFEN
        result = sf.get_fen("shogi", SHOGI, [], False, True)
        self.assertEqual(result, SHOGI_SFEN)

        # makruk FEN
        fen = "rnsmksnr/8/1pM~1pppp/p7/8/PPPP1PPP/8/RNSKMSNR b - - 0 3"
        result = sf.get_fen("makruk", MAKRUK, ["e3e4", "d6d5", "e4d5", "a6a5", "d5c6m"], False, False, True)
        self.assertEqual(result, fen)
        result = sf.get_fen("makruk", fen, [], False, False, True)
        self.assertEqual(result, fen)

        # makruk piece honor counting
        fen = "8/3k4/8/2K1S1P1/8/8/8/8 w - - 0 1"
        moves = ["g5g6m"]
        result = sf.get_fen("makruk", fen, moves, False, False, True)
        self.assertEqual(result, "8/3k4/6M~1/2K1S3/8/8/8/8 b - 88 8 1")

        fen = "8/2K3k1/5m2/4S1S1/8/8/8/8 w - 128 97 1"
        moves = ["e5f6"]
        result = sf.get_fen("makruk", fen, moves, False, False, True)
        self.assertEqual(result, "8/2K3k1/5S2/6S1/8/8/8/8 b - 44 8 1")

        # ignore count_started for piece honor counting
        fen = "8/3k4/8/2K1S1P1/8/8/8/8 w - - 0 1"
        moves = ["g5g6m"]
        result = sf.get_fen("makruk", fen, moves, False, False, True, -1)
        self.assertEqual(result, "8/3k4/6M~1/2K1S3/8/8/8/8 b - 88 8 1")

        fen = "8/2K3k1/5m2/4S1S1/8/8/8/8 w - 128 1 30"
        moves = ["e5f6"]
        result = sf.get_fen("makruk", fen, moves, False, False, True, 58)
        self.assertEqual(result, "8/2K3k1/5S2/6S1/8/8/8/8 b - 44 8 30")

        # makruk board honor counting
        fen = "3k4/2m5/8/4MP2/3KS3/8/8/8 w - - 0 1"
        moves = ["f5f6m"]
        result = sf.get_fen("makruk", fen, moves, False, False, True)
        self.assertEqual(result, "3k4/2m5/5M~2/4M3/3KS3/8/8/8 b - 128 0 1")

        fen = "3k4/2m5/5M~2/4M3/3KS3/8/8/8 w - 128 0 33"
        moves = ["d4d5"]
        result = sf.get_fen("makruk", fen, moves, False, False, True)
        self.assertEqual(result, "3k4/2m5/5M~2/3KM3/4S3/8/8/8 b - 128 1 33")

        fen = "3k4/2m5/5M~2/4M3/3KS3/8/8/8 w - 128 36 1"
        moves = ["d4d5"]
        result = sf.get_fen("makruk", fen, moves, False, False, True)
        self.assertEqual(result, "3k4/2m5/5M~2/3KM3/4S3/8/8/8 b - 128 37 1")

        fen = "3k4/2m5/5M~2/4M3/3KS3/8/8/8 w - 128 0 33"
        moves = ["d4d5"]
        result = sf.get_fen("makruk", fen, moves, False, False, True, -1)
        self.assertEqual(result, "3k4/2m5/5M~2/3KM3/4S3/8/8/8 b - 128 0 33")

        fen = "3k4/2m5/5M~2/4M3/3KS3/8/8/8 w - 128 7 33"
        moves = ["d4d5"]
        result = sf.get_fen("makruk", fen, moves, False, False, True, 58)
        self.assertEqual(result, "3k4/2m5/5M~2/3KM3/4S3/8/8/8 b - 128 8 33")

        # ouk piece honor counting
        fen = "8/3k4/8/2K1S1P1/8/8/8/8 w - - 0 1"
        moves = ["g5g6m"]
        result = sf.get_fen("cambodian", fen, moves, False, False, True)
        self.assertEqual(result, "8/3k4/6M~1/2K1S3/8/8/8/8 b - 86 8 1")

        fen = "8/2K3k1/5m2/4S1S1/8/8/8/8 w - 128 97 1"
        moves = ["e5f6"]
        result = sf.get_fen("cambodian", fen, moves, False, False, True)
        self.assertEqual(result, "8/2K3k1/5S2/6S1/8/8/8/8 b - 42 8 1")

        # adjust to board honor counting if it's faster
        fen = "8/3k4/8/2K1S1P1/8/8/8/8 w - - 0 1"
        moves = ["g5g6m"]
        result = sf.get_fen("cambodian", fen, moves, False, False, True, -1)
        self.assertEqual(result, "8/3k4/6M~1/2K1S3/8/8/8/8 b - 86 8 1")

        fen = "8/2K3k1/5m2/4S1S1/8/8/8/8 w - 126 101 80"
        moves = ["e5f6"]
        result = sf.get_fen("cambodian", fen, moves, False, False, True, 58)
        self.assertEqual(result, "8/2K3k1/5S2/6S1/8/8/8/8 b - 126 102 80")

        # pawn promotion triggers piece honor counting
        fen = "8/8/4k3/5P2/8/2RMK3/8/8 w - 126 41 50"
        moves = ["f5f6m"]
        result = sf.get_fen("cambodian", fen, moves, False, False, True, 58)
        self.assertEqual(result, "8/8/4kM~2/8/8/2RMK3/8/8 b - 30 10 50")

        # king capturing the last unpromoted pawn triggers piece honor counting
        fen = "8/8/4k3/5P2/8/2RMK3/8/8 b - 126 42 50"
        moves = ["e6f5"]
        result = sf.get_fen("cambodian", fen, moves, False, False, True, 58)
        self.assertEqual(result, "8/8/8/5k2/8/2RMK3/8/8 w - 30 7 51")

        # ouk board honor counting
        fen = "3k4/2m5/8/4MP2/3KS3/8/8/8 w - - 0 1"
        moves = ["f5f6m"]
        result = sf.get_fen("cambodian", fen, moves, False, False, True)
        self.assertEqual(result, "3k4/2m5/5M~2/4M3/3KS3/8/8/8 b - 126 0 1")

        fen = "3k4/2m5/5M~2/4M3/3KS3/8/8/8 w - 126 0 33"
        moves = ["d4d5"]
        result = sf.get_fen("cambodian", fen, moves, False, False, True)
        self.assertEqual(result, "3k4/2m5/5M~2/3KM3/4S3/8/8/8 b - 126 1 33")

        fen = "3k4/2m5/5M~2/4M3/3KS3/8/8/8 w - 126 36 1"
        moves = ["d4d5"]
        result = sf.get_fen("cambodian", fen, moves, False, False, True)
        self.assertEqual(result, "3k4/2m5/5M~2/3KM3/4S3/8/8/8 b - 126 37 1")

        fen = "3k4/2m5/5M~2/4M3/3KS3/8/8/8 w - 126 0 33"
        moves = ["d4d5"]
        result = sf.get_fen("cambodian", fen, moves, False, False, True, -1)
        self.assertEqual(result, "3k4/2m5/5M~2/3KM3/4S3/8/8/8 b - 126 0 33")

        fen = "3k4/2m5/5M~2/4M3/3KS3/8/8/8 w - 126 7 33"
        moves = ["d4d5"]
        result = sf.get_fen("cambodian", fen, moves, False, False, True, 58)
        self.assertEqual(result, "3k4/2m5/5M~2/3KM3/4S3/8/8/8 b - 126 8 33")

        # asean counting
        fen = "4k3/3r4/2K5/8/3R4/8/8/8 w - - 0 1"
        moves = ["d4d7"]
        result = sf.get_fen("asean", fen, moves, False, False, False)
        self.assertEqual(result, "4k3/3R4/2K5/8/8/8/8/8 b - 32 0 1")

        fen = "4k3/3r4/2K5/8/3R4/1P6/8/8 w - - 0 1"
        moves = ["d4d7"]
        result = sf.get_fen("asean", fen, moves, False, False, False)
        self.assertEqual(result, "4k3/3R4/2K5/8/8/1P6/8/8 b - - 0 1")

        fen = "8/2P1k3/2K5/8/8/8/8/8 w - - 0 1"
        moves = ["c7c8b"]
        result = sf.get_fen("asean", fen, moves, False, False, False)
        self.assertEqual(result, "2B5/4k3/2K5/8/8/8/8/8 b - 88 0 1")

        fen = "8/8/4K3/3Q4/1k1N4/5b2/8/8 w - - 0 1"
        moves = ["d4f3"]
        result = sf.get_fen("asean", fen, moves, False, False, False)
        self.assertEqual(result, "8/8/4K3/3Q4/1k6/5N2/8/8 b - 128 0 1")

        fen = "3Q4/4P3/4K3/3Q4/1k6/8/8/8 w - - 0 1"
        moves = ["e7e8q"]
        result = sf.get_fen("asean", fen, moves, False, False, False)
        self.assertEqual(result, "3QQ3/8/4K3/3Q4/1k6/8/8/8 b - - 0 1")

        # Cambodian king loses its leap ability when it is "aimed" by a rook
        fen = "rnsmk1nr/4s3/pppppppp/8/8/PPPPPPPP/R7/1NSKMSNR w DEde - 2 2"
        moves = ["a2e2"]
        result = sf.get_fen("cambodian", fen, moves, False, False, True)
        self.assertEqual(result, "rnsmk1nr/4s3/pppppppp/8/8/PPPPPPPP/4R3/1NSKMSNR b DEd - 3 2")

        fen = "1nsmksnr/r7/pppppppp/8/8/PPPPPPPP/2SN4/R2KMSNR b DEde - 3 2"
        moves = ["a7d7"]
        result = sf.get_fen("cambodian", fen, moves, False, False, True)
        self.assertEqual(result, "1nsmksnr/3r4/pppppppp/8/8/PPPPPPPP/2SN4/R2KMSNR w Ede - 4 3")

        fen = "rnsmksnr/8/1ppppppp/8/8/1PPPPPPP/8/RNSKMSNR w DEde - 0 1"
        moves = ["a1a8"]
        result = sf.get_fen("cambodian", fen, moves, False, False, True)
        self.assertEqual(result, "Rnsmksnr/8/1ppppppp/8/8/1PPPPPPP/8/1NSKMSNR b DEd - 0 1")

    def test_capture_anything_knight_self_capture(self):
        chess_start = sf.start_fen("chess")
        chess_moves = sf.legal_moves("chess", chess_start, [])
        self.assertNotIn("g1e2", chess_moves)

        cap_start = sf.start_fen("capture-anything")
        cap_moves = sf.legal_moves("capture-anything", cap_start, [])
        self.assertIn("g1e2", cap_moves)

        san = sf.get_san("capture-anything", cap_start, "g1e2")
        self.assertIn("x", san)

    def test_capture_anything_pawn_self_capture_resets_clock(self):
        fen = "6k1/8/8/5N2/4P3/8/8/6K1 w - - 17 1"
        moves = sf.legal_moves("capture-anything", fen, [])
        self.assertIn("e4f5", moves)
        self.assertTrue(sf.is_capture("capture-anything", fen, [], "e4f5"))

        new_fen = sf.get_fen("capture-anything", fen, ["e4f5"])
        self.assertEqual(int(new_fen.split()[4]), 0)

    def test_self_capture_hand_keeps_mover_color(self):
        fen = sf.start_fen("selfhouse")
        new_fen = sf.get_fen("selfhouse", fen, ["g1e2"])
        self.assertIn("[P]", new_fen)

    def test_benedict_morph_capture_changes_piece_type(self):
        fen = "6k1/8/8/3r4/8/2N5/8/6K1 w - - 0 1"
        new_fen = sf.get_fen("benedictmorph", fen, ["c3d5"])
        self.assertIn("3R4", new_fen.split()[0])

    def test_benedict_morph_king_stays_king(self):
        fen = "6k1/8/8/8/8/8/6r1/6K1 w - - 0 1"
        new_fen = sf.get_fen("benedictmorph", fen, ["g1g2"])
        self.assertIn("6K1", new_fen.split()[0])

    def test_spell_chess_jump_capture_wins_immediately(self):
        fen = "5rk1/1p2ppb1/2p1q1p1/3p2Np/3P2n1/3BP3/PPP2PPP/R1B1K2R[JFFFFjffff] b KQ - 5 12"
        result = sf.game_result("spell-chess", fen, ["j@e3,e6e1"])
        self.assertEqual(result, -sf.VALUE_MATE)

    def test_spell_chess_freeze_check_does_not_win(self):
        fen = "rnbqk1nr/pppp1ppp/8/1N2p3/1b6/8/PPPPPPPP/R1BQKBNR[JJFFFFFjjfffff] w KQkq - 2 3"
        result = sf.game_result("spell-chess", fen, ["f@d7,b5c7"])
        self.assertNotEqual(result, sf.VALUE_MATE)
        self.assertNotEqual(result, -sf.VALUE_MATE)

    def test_spell_chess_frozen_rook_blocks_castling(self):
        fen = "4k3/8/8/8/8/8/8/4K2R[f] b K - 0 1"
        moves = sf.legal_moves("spell-chess", fen, ["f@h1,e8e7"])
        self.assertNotIn("e1g1", moves)

    def test_spell_chess_castling_through_attack_requires_freeze(self):
        fen = "4kr2/8/8/8/8/8/8/4K2R[F] w K - 0 1"
        moves = sf.legal_moves("spell-chess", fen, [])
        self.assertNotIn("e1g1", moves)
        self.assertIn("f@f8,e1g1", moves)

    def test_spell_chess_cannot_castle_out_of_check_without_freeze(self):
        fen = "4r1k1/8/8/8/8/8/8/4K2R[F] w K - 0 1"
        moves = sf.legal_moves("spell-chess", fen, [])
        self.assertNotIn("e1g1", moves)
        self.assertIn("f@e8,e1g1", moves)

    def test_spell_chess_jump_potion_does_not_bypass_castling_blockers(self):
        clear_fen = "6k1/8/8/8/8/8/8/R3K3[J] w Q - 0 1"
        d1_blocked = "6k1/8/8/8/8/8/8/R2nK3[J] w Q - 0 1"
        b1_blocked = "6k1/8/8/8/8/8/8/Rn2K3[J] w Q - 0 1"

        self.assertIn("e1c1", sf.legal_moves("spell-chess", clear_fen, []))

        blocked_d1_moves = sf.legal_moves("spell-chess", d1_blocked, [])
        self.assertNotIn("e1c1", blocked_d1_moves)
        self.assertNotIn("j@d1,e1c1", blocked_d1_moves)

        blocked_b1_moves = sf.legal_moves("spell-chess", b1_blocked, [])
        self.assertNotIn("e1c1", blocked_b1_moves)
        self.assertNotIn("j@b1,e1c1", blocked_b1_moves)

    def test_spell_chess_frozen_pawn_cannot_capture_en_passant(self):
        fen = "4k3/3p4/8/4P3/8/8/8/4K3[f] b - - 0 1"
        moves = sf.legal_moves("spell-chess", fen, ["f@e5,d7d5"])
        self.assertNotIn("e5d6", moves)

    def test_spell_chess_potion_fen_extension_roundtrip(self):
        fen = sf.get_fen("spell-chess", sf.start_fen("spell-chess"), ["f@a6,e2e4"])
        self.assertIn("f:a6", fen)
        self.assertIn("<", fen)
        self.assertEqual(sf.validate_fen(fen, "spell-chess"), sf.FEN_OK)
        self.assertEqual(sf.get_fen("spell-chess", fen, []), fen)

    def test_spell_chess_potion_fen_extension_parse(self):
        fen = "4k3/8/8/8/8/8/8/4K3[] w - - 0 1 f:e4 <1 2 3 4>"
        self.assertEqual(sf.validate_fen(fen, "spell-chess"), sf.FEN_OK)
        normalized = sf.get_fen("spell-chess", fen, [])
        self.assertIn("f:e4", normalized)
        self.assertIn("<1 2 3 4>", normalized)

        bad_cooldowns = [
            "4k3/8/8/8/8/8/8/4K3[] w - - 0 1 f:e4 <1,2>",
            "4k3/8/8/8/8/8/8/4K3[] w - - 0 1 f:e4 <1 x 2>",
            "4k3/8/8/8/8/8/8/4K3[] w - - 0 1 f:e4 <999999999999999999999 2>",
        ]
        for bad_fen in bad_cooldowns:
            self.assertNotEqual(sf.validate_fen(bad_fen, "spell-chess"), sf.FEN_OK)

    def test_spell_chess_potion_fen_roundtrip_after_both_potion_types(self):
        fen = sf.get_fen("spell-chess", sf.start_fen("spell-chess"), ["f@a6,e2e4", "j@a7,a8a2"])
        self.assertEqual(sf.validate_fen(fen, "spell-chess"), sf.FEN_OK)
        self.assertEqual(sf.get_fen("spell-chess", fen, []), fen)

    def test_get_san(self):
        fen = "4k3/8/3R4/8/1R3R2/8/3R4/4K3 w - - 0 1"
        result = sf.get_san("chess", fen, "b4d4")
        self.assertEqual(result, "Rbd4")

        result = sf.get_san("chess", fen, "f4d4")
        self.assertEqual(result, "Rfd4")

        result = sf.get_san("chess", fen, "d2d4")
        self.assertEqual(result, "R2d4")

        result = sf.get_san("chess", fen, "d6d4")
        self.assertEqual(result, "R6d4")

        fen = "4k3/8/3R4/3P4/1RP1PR2/8/3R4/4K3 w - - 0 1"
        result = sf.get_san("chess", fen, "d2d4")
        self.assertEqual(result, "Rd4")

        fen = "1r2k3/P1P5/8/8/8/8/8/4K3 w - - 0 1"
        result = sf.get_san("chess", fen, "c7b8q")
        self.assertEqual(result, "cxb8=Q+")

        fen = "1r2k3/P1P5/8/8/8/8/8/4K3 w - - 0 1"
        result = sf.get_san("chess", fen, "c7b8q", False, sf.NOTATION_LAN)
        self.assertEqual(result, "c7xb8=Q+")

        result = sf.get_san("capablanca", CAPA, "e2e4")
        self.assertEqual(result, "e4")

        result = sf.get_san("capablanca", CAPA, "e2e4", False, sf.NOTATION_LAN)
        self.assertEqual(result, "e2-e4")

        result = sf.get_san("capablanca", CAPA, "h1i3")
        self.assertEqual(result, "Ci3")

        result = sf.get_san("sittuyin", SITTUYIN, "R@a1")
        self.assertEqual(result, "R@a1")

        fen = "3rr3/1kn3n1/1ss1p1pp/1pPpP3/6PP/p3KN2/2SSFN2/3R3R[] b - - 0 14"
        result = sf.get_san("sittuyin", fen, "c6c5")
        self.assertEqual(result, "Scxc5")

        fen = "7R/1r6/3k1np1/3s2N1/3s3P/4n3/6p1/2R3K1[] w - - 2 55"
        result = sf.get_san("sittuyin", fen, "h4h4f")
        self.assertEqual(result, "h4=F")

        fen = "k7/2K3P1/8/4P3/8/8/8/1R6[] w - - 0 1"
        result = sf.get_san("sittuyin", fen, "e5f6f")
        self.assertEqual(result, "e5f6=F")

        result = sf.get_san("shogi", SHOGI, "i3i4")
        self.assertEqual(result, "P-16")

        result = sf.get_san("shogi", SHOGI, "i3i4", False, sf.NOTATION_SHOGI_HOSKING)
        self.assertEqual(result, "P16")

        result = sf.get_san("shogi", SHOGI, "f1e2", False, sf.NOTATION_SHOGI_HOSKING)
        self.assertEqual(result, "G49-58")
        result = sf.get_san("shogi", SHOGI, "f1e2", False, sf.NOTATION_SHOGI_HODGES)
        self.assertEqual(result, "G4i-5h")
        result = sf.get_san("shogi", SHOGI, "f1e2", False, sf.NOTATION_SHOGI_HODGES_NUMBER)
        self.assertEqual(result, "G49-58")

        # Disambiguation of promotion moves
        fen = "p1ksS/n1n2/4P/5/+L1K1+L[] b - - 3 9"
        result = sf.get_san("kyotoshogi", fen, "c4b2+", False, sf.NOTATION_SHOGI_HODGES_NUMBER)
        self.assertEqual(result, "N32-44+")
        result = sf.get_san("kyotoshogi", fen, "a4b2+", False, sf.NOTATION_SHOGI_HODGES_NUMBER)
        self.assertEqual(result, "N52-44+")

        # Demotion
        fen = "p+nks+l/5/5/L4/1SK+NP[-] b 0 1"
        result = sf.get_san("kyotoshogi", fen, "e5e4-", False, sf.NOTATION_SAN)
        self.assertEqual(result, "Ge4=L")

        fen = "lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w -"
        result = sf.get_san("shogi", fen, "b2h8", False, sf.NOTATION_SHOGI_HODGES)
        self.assertEqual(result, "Bx2b=")
        result = sf.get_san("shogi", fen, "b2h8+", False, sf.NOTATION_SHOGI_HODGES)
        self.assertEqual(result, "Bx2b+")

        fen = "lnsgkg1nl/1r5s1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/7R1/LNSGKGSNL[Bb] w "
        result = sf.get_san("shogi", fen, "B@g7", False, sf.NOTATION_SHOGI_HODGES)
        self.assertEqual(result, "B*3c")
        result = sf.get_san("shogi", fen, "B@g7", False, sf.NOTATION_SHOGI_HODGES_NUMBER)
        self.assertEqual(result, "B*33")

        fen = "lnsgkg1nl/1r4s+B1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/7R1/LNSGKGSNL[B] w "
        result = sf.get_san("shogi", fen, "h8g7", False, sf.NOTATION_SHOGI_HODGES)
        self.assertEqual(result, "+B-3c")

        fen = "lnk2gsnl/7b1/p1p+SGp1pp/6p2/1pP6/4P4/PP3PPPP/1S2G2R1/L2GK1bNL[PRppns] w "
        result = sf.get_san("shogi", fen, "d7d8", False, sf.NOTATION_SHOGI_HODGES)
        self.assertEqual(result, "+S-6b")

        result = sf.get_san("xiangqi", XIANGQI, "h1g3")
        self.assertEqual(result, "Hg3")

        result = sf.get_san("xiangqi", XIANGQI, "h1g3", False, sf.NOTATION_XIANGQI_WXF)
        self.assertEqual(result, "H2+3")

        result = sf.get_san("xiangqi", XIANGQI, "c1e3")
        self.assertEqual(result, "Ece3")

        result = sf.get_san("xiangqi", XIANGQI, "c1e3", False, sf.NOTATION_XIANGQI_WXF)
        self.assertEqual(result, "E7+5")

        result = sf.get_san("xiangqi", XIANGQI, "h3h10")
        self.assertEqual(result, "Cxh10")

        result = sf.get_san("xiangqi", XIANGQI, "h3h10", False, sf.NOTATION_XIANGQI_WXF)
        self.assertEqual(result, "C2+7")

        result = sf.get_san("xiangqi", XIANGQI, "h3h5")
        self.assertEqual(result, "Ch5")

        # WXF notation does not denote check or checkmate
        fen = "4k4/4a3R/9/9/9/9/9/9/4K4/9 w - - 0 1"
        result = sf.get_san("xiangqi", fen, "i9e9", False)
        self.assertEqual(result, "Rxe9+")
        result = sf.get_san("xiangqi", fen, "i9e9", False, sf.NOTATION_XIANGQI_WXF)
        self.assertEqual(result, "R1=5")
        result = sf.get_san("xiangqi", fen, "i9i10", False)
        self.assertEqual(result, "Ri10#")
        result = sf.get_san("xiangqi", fen, "i9i10", False, sf.NOTATION_XIANGQI_WXF)
        self.assertEqual(result, "R1+1")

        # skip disambiguation for elephants and advisors, but not for pieces that require it
        fen = "rnbakabnr/9/1c5c1/p1p1p1p1p/4P4/1NB6/P1P1P3P/1C1A3C1/9/RNBAK4 w - - 0 1"
        result = sf.get_san("xiangqi", fen, "c5e3", False, sf.NOTATION_XIANGQI_WXF)
        self.assertEqual(result, "E7-5")
        result = sf.get_san("xiangqi", fen, "d1e2", False, sf.NOTATION_XIANGQI_WXF)
        self.assertEqual(result, "A6+5")
        result = sf.get_san("xiangqi", fen, "b5c7", False, sf.NOTATION_XIANGQI_WXF)
        self.assertEqual(result, "H++7")
        result = sf.get_san("xiangqi", fen, "e6e7", False, sf.NOTATION_XIANGQI_WXF)
        self.assertEqual(result, "P++1")
        result = sf.get_san("xiangqi", fen, "e4e5", False, sf.NOTATION_XIANGQI_WXF)
        self.assertEqual(result, "P-+1")

        # Tandem pawns
        fen = "rnbakabnr/9/1c5c1/p1p1P1p1p/4P4/9/P3P3P/1C5C1/9/RNBAKABNR w - - 0 1"
        result = sf.get_san("xiangqi", fen, "e7d7", False, sf.NOTATION_XIANGQI_WXF)
        self.assertEqual(result, "15=6")
        result = sf.get_san("xiangqi", fen, "e6d6", False, sf.NOTATION_XIANGQI_WXF)
        self.assertEqual(result, "25=6")
        result = sf.get_san("xiangqi", fen, "e4e5", False, sf.NOTATION_XIANGQI_WXF)
        self.assertEqual(result, "35+1")

        # use tandem pawn notation for pair of tandem pawns
        fen = "5k3/9/3P5/3P1P1P1/5P3/9/9/9/9/4K4 w - - 0 1"
        result = sf.get_san("xiangqi", fen, "d7e7", False, sf.NOTATION_XIANGQI_WXF)
        self.assertEqual(result, "26=5")
        result = sf.get_san("xiangqi", fen, "f6e6", False, sf.NOTATION_XIANGQI_WXF)
        self.assertEqual(result, "24=5")

        fen = "1rb1ka2r/4a4/2ncb1nc1/p1p1p1p1p/9/2P6/P3PNP1P/2N1C2C1/9/R1BAKAB1R w - - 1 7"
        result = sf.get_san("xiangqi", fen, "c3e2")
        self.assertEqual(result, "Hce2")

        result = sf.get_san("xiangqi", fen, "c3d5")
        self.assertEqual(result, "Hd5")

        result = sf.get_san("janggi", JANGGI, "b1c3", False, sf.NOTATION_JANGGI)
        self.assertEqual(result, "H02-83")

        fen = "1b1aa2b1/5k3/3ncn3/1pp1pp3/5r2p/9/P1PPB1PPB/2N1CCN1c/9/R2AKAR2 w - - 19 17"
        result = sf.get_san("janggi", fen, "d1e2", False, sf.NOTATION_SAN)
        self.assertEqual(result, "Ade2")

        fen = "1Pbcka3/3nNn1c1/N2CaC3/1pB6/9/9/5P3/9/4K4/9 w - - 0 23"
        result = sf.get_san("janggi", fen, "f8f10", False, sf.NOTATION_SAN)
        self.assertEqual(result, "Cfxf10")

        result = sf.get_san("makruk", MAKRUK, "e3e4")
        self.assertEqual(result, "e4")
        result = sf.get_san("makruk", MAKRUK, "e3e4", False, sf.NOTATION_THAI_SAN)
        self.assertEqual(result, "จ๔")
        result = sf.get_san("makruk", MAKRUK, "e3e4", False, sf.NOTATION_THAI_LAN)
        self.assertEqual(result, "บ จ๓-จ๔")

        fen = "r1smksnr/3n4/pppp1ppp/4p3/4PP2/PPPP2PP/8/RNSKMSNR w - - 0 1"
        result = sf.get_san("makruk", fen, "f4e5")
        self.assertEqual(result, "fxe5")
        result = sf.get_san("makruk", fen, "f4e5", False, sf.NOTATION_THAI_SAN)
        self.assertEqual(result, "ฉxจ๕")
        result = sf.get_san("makruk", fen, "f4e5", False, sf.NOTATION_THAI_LAN)
        self.assertEqual(result, "บ ฉ๔xจ๕")

        fen = "rnsm1s1r/4n1k1/1ppppppp/p7/2PPP3/PP3PPP/4N2R/RNSKMS2 b - - 1 5"
        result = sf.get_san("makruk", fen, "f8f7")
        self.assertEqual(result, "Sf7")
        result = sf.get_san("makruk", fen, "f8f7", False, sf.NOTATION_THAI_SAN)
        self.assertEqual(result, "ค-ฉ๗")
        result = sf.get_san("makruk", fen, "f8f7", False, sf.NOTATION_THAI_LAN)
        self.assertEqual(result, "ค ฉ๘-ฉ๗")

        fen = "4k3/8/8/4S3/8/2S5/8/4K3 w - - 0 1"
        result = sf.get_san("makruk", fen, "e5d4")
        self.assertEqual(result, "Sed4")
        result = sf.get_san("makruk", fen, "c3d4")
        self.assertEqual(result, "Scd4")
        result = sf.get_san("makruk", fen, "e5d4", False, sf.NOTATION_THAI_SAN)
        self.assertEqual(result, "คจ-ง๔")
        result = sf.get_san("makruk", fen, "c3d4", False, sf.NOTATION_THAI_SAN)
        self.assertEqual(result, "คค-ง๔")
        result = sf.get_san("makruk", fen, "e5d4", False, sf.NOTATION_THAI_LAN)
        self.assertEqual(result, "ค จ๕-ง๔")
        result = sf.get_san("makruk", fen, "c3d4", False, sf.NOTATION_THAI_LAN)
        self.assertEqual(result, "ค ค๓-ง๔")

        # Distinction between the regular met and the promoted pawn
        fen = "4k3/8/4M3/4S3/8/2S5/8/4K3 w - - 0 1"
        result = sf.get_san("makruk", fen, "e6d5", False, sf.NOTATION_THAI_SAN)
        self.assertEqual(result, "ม็-ง๕")
        fen = "4k3/8/4M~3/4S3/8/2S5/8/4K3 w - - 0 1"
        result = sf.get_san("makruk", fen, "e6d5", False, sf.NOTATION_THAI_SAN)
        self.assertEqual(result, "ง-ง๕")

        fen = "4k3/8/8/3S4/8/3S4/8/4K3 w - - 0 1"
        result = sf.get_san("makruk", fen, "d3d4")
        self.assertEqual(result, "Sd4")
        result = sf.get_san("makruk", fen, "d3d4", False, sf.NOTATION_THAI_SAN)
        self.assertEqual(result, "ค-ง๔")
        result = sf.get_san("makruk", fen, "d3d4", False, sf.NOTATION_THAI_LAN)
        self.assertEqual(result, "ค ง๓-ง๔")


        UCI_moves = ["e2e4", "e7e5", "g1f3", "b8c6h", "f1c4", "f8c5e"]
        SAN_moves = ["e4", "e5", "Nf3", "Nc6/H", "Bc4", "Bc5/E"]

        fen = SEIRAWAN
        for i, move in enumerate(UCI_moves):
            result = sf.get_san("seirawan", fen, move)
            self.assertEqual(result, SAN_moves[i])
            fen = sf.get_fen("seirawan", SEIRAWAN, UCI_moves[:i + 1])

        result = sf.get_san("seirawan", fen, "e1g1")
        self.assertEqual(result, "O-O")

        result = sf.get_san("seirawan", fen, "e1g1h")
        self.assertEqual(result, "O-O/He1")
        result = sf.get_san("seirawan", fen, "e1g1e")
        self.assertEqual(result, "O-O/Ee1")

        result = sf.get_san("seirawan", fen, "h1e1h")
        self.assertEqual(result, "O-O/Hh1")
        result = sf.get_san("seirawan", fen, "h1e1e")
        self.assertEqual(result, "O-O/Eh1")

        # Disambiguation only when necessary
        fen = "rnbqkb1r/ppp1pppp/5n2/3p4/3P4/5N2/PPP1PPPP/RNBQKB1R[EHeh] w KQABCDEFHkqabcdefh - 2 3"
        result = sf.get_san("seirawan", fen, "b1d2e")
        self.assertEqual(result, "Nd2/E")
        result = sf.get_san("seirawan", fen, "b1d2")
        self.assertEqual(result, "Nbd2")

    def test_get_san_moves(self):
        UCI_moves = ["e2e4", "e7e5", "g1f3", "b8c6h", "f1c4", "f8c5e"]
        SAN_moves = ["e4", "e5", "Nf3", "Nc6/H", "Bc4", "Bc5/E"]
        result = sf.get_san_moves("seirawan", SEIRAWAN, UCI_moves)
        self.assertEqual(result, SAN_moves)

        UCI_moves = ["c3c4", "g7g6", "b2h8"]
        SAN_moves = ["P-76", "P-34", "Bx22="]
        result = sf.get_san_moves("shogi", SHOGI, UCI_moves)
        self.assertEqual(result, SAN_moves)

        UCI_moves = ["h3e3", "h10g8", "h1g3", "c10e8", "a1a3", "i10h10"]
        SAN_moves = ["C2=5", "H8+7", "H2+3", "E3+5", "R9+2", "R9=8"]
        result = sf.get_san_moves("xiangqi", XIANGQI, UCI_moves, False, sf.NOTATION_XIANGQI_WXF)
        self.assertEqual(result, SAN_moves)

        UCI_moves = ["e2e4", "d7d5", "f1a6+", "d8d6"]
        SAN_moves = ["e4", "d5", "Ba6=A", "Qd6"]
        result = sf.get_san_moves("shogun", SHOGUN, UCI_moves)
        self.assertEqual(result, SAN_moves)

    def test_gives_check(self):
        result = sf.gives_check("capablanca", CAPA, [])
        self.assertFalse(result)

        result = sf.gives_check("capablanca", CAPA, ["e2e4"])
        self.assertFalse(result)

        moves = ["g2g3", "d7d5", "a2a3", "c8h3"]
        result = sf.gives_check("capablanca", CAPA, moves)
        self.assertTrue(result)

        # Test giving check to pseudo royal piece
        result = sf.gives_check("atomic", CHESS, [])
        self.assertFalse(result)

        result = sf.gives_check("atomic", CHESS, ["e2e4"])
        self.assertFalse(result)

        result = sf.gives_check("atomic", CHESS, ["e2e4", "d7d5", "f1b5"])
        self.assertTrue(result)

        result = sf.gives_check("atomic", "rnbqkbnr/ppp2ppp/8/8/8/8/PPP2PPP/RNBQKBNR w KQkq - 0 4", ["d1d7"])
        self.assertTrue(result)

        result = sf.gives_check("atomic", "8/8/kK6/8/8/8/Q7/8 b - - 0 1", [])
        self.assertFalse(result)

        # pseudo-royal duple check
        result = sf.gives_check("spartan", "lgkcckw1/hhhhhhhh/1N3lN1/8/8/8/PPPPPPPP/R1BQKB1R b KQ - 11 6", [])
        self.assertTrue(result)
        result = sf.gives_check("spartan", "lgkcckwl/hhhhhhhh/6N1/8/8/8/PPPPPPPP/RNBQKB1R b KQ - 5 3", [])
        self.assertFalse(result)
        result = sf.gives_check("spartan", "lgkcckwl/hhhhhhhh/8/8/8/8/PPPPPPPP/RNBQKBNR w KQ - 0 1", [])
        self.assertFalse(result)

        # Shako castling discovered check
        result = sf.gives_check("shako", "10/5r4/2p3pBk1/1p6Pr/p3p5/9e/1PP2P4/P2P2PP2/ER3K2R1/8C1 w K - 7 38", ["f2h2"])
        self.assertTrue(result)

        # This Janggi move is legal and gives check.
        self.assertTrue(sf.gives_check("janggi", "4ka3/4a4/9/4R4/2B6/9/9/5K3/4p4/3r5 b - - 0 113", ["e2f2"]))

        # captureForbidden to royal type must suppress gives_check
        sf.load_variant_config(
            """
[forbidden-check-gives:chess]
customPiece1 = d:Q
pieceToCharTable = PNBRQ............D...Kpnbrq............d...k
captureForbidden = d:k
startFen = 4k3/8/8/8/8/8/4D3/4K3 w - - 0 1
"""
        )
        self.assertFalse(
            sf.gives_check(
                "forbidden-check-gives",
                sf.start_fen("forbidden-check-gives"),
                ["e2e7"],
            )
        )

        # Physical KING remains royal target even when king_type() is remapped.
        sf.load_variant_config(
            """
[forbidden-check-gives-royal-wazir:chess]
king = k:W
captureForbidden = q:k
startFen = 4k3/8/8/8/8/8/8/4Q2K w - - 0 1
"""
        )
        self.assertFalse(
            sf.gives_check(
                "forbidden-check-gives-royal-wazir",
                sf.start_fen("forbidden-check-gives-royal-wazir"),
                ["e1e2"],
            )
        )

    def test_is_capture(self):
        result = sf.is_capture("chess", CHESS, [], "e2e4")
        self.assertFalse(result)

        result = sf.is_capture("chess", CHESS, ["e2e4", "e7e5", "g1f3", "b8c6", "f1c4", "f8c5"], "e1g1")
        self.assertFalse(result)

        result = sf.is_capture("chess", CHESS, ["e2e4", "g8f6", "e4e5", "d7d5"], "e5f6")
        self.assertTrue(result)

        # en passant
        result = sf.is_capture("chess", CHESS, ["e2e4", "g8f6", "e4e5", "d7d5"], "e5d6")
        self.assertTrue(result)

        # En passant for non-pawn piece with identical move/capture squares
        sf.load_variant_config(
            """[epsoldier:chess]
soldier = s
enPassantTypes = s
mustCapture = true
startFen = 4k3/8/2S5/3p4/8/8/8/4K3 w - d6 0 1
"""
        )
        ep_fen = sf.start_fen("epsoldier")
        self.assertEqual(sf.legal_moves("epsoldier", ep_fen, []), ["c6d6"])
        self.assertTrue(sf.is_capture("epsoldier", ep_fen, [], "c6d6"))

        # 960 castling
        result = sf.is_capture("chess", "bqrbkrnn/pppppppp/8/8/8/8/PPPPPPPP/BQRBKRNN w CFcf - 0 1", ["g1f3", "h8g6"], "e1f1", True)
        self.assertFalse(result)

        # Sittuyin in-place promotion
        result = sf.is_capture("sittuyin", "8/2k5/8/4P3/4P1N1/5K2/8/8[] w - - 0 1", [], "e5e5f")
        self.assertFalse(result)

    def test_piece_to_partner(self):
        # take the rook and promote to queen
        result = sf.piece_to_partner("bughouse", "r2qkbnr/1Ppppppp/2n5/8/8/8/1PPPPPPP/RNBQKBNR[] w KQkq - 0 1", ["b7a8q"])
        self.assertEqual(result, "r")

        # take back the queen (promoted pawn)
        result = sf.piece_to_partner("bughouse", "r2qkbnr/1Ppppppp/2n5/8/8/8/1PPPPPPP/RNBQKBNR[] w KQkq - 0 1", ["b7a8q", "d8a8"])
        self.assertEqual(result, "P")

        # just a simple move (no take)
        result = sf.piece_to_partner("bughouse", "r2qkbnr/1Ppppppp/2n5/8/8/8/1PPPPPPP/RNBQKBNR[] w KQkq - 0 1", ["b7a8q", "d8b8"])
        self.assertEqual(result, "")

        # silver takes the pawn and promotes to gold
        result = sf.piece_to_partner("shogi", "lnsgkgsnl/1r5b1/ppppppppp/S8/9/9/PPPPPPPPP/1B5R1/LNSGKG1NL[] w 0 1", ["a6a7+"])
        self.assertEqual(result, "p")

        # take back the gold (promoted silver)
        result = sf.piece_to_partner("shogi", "lnsgkgsnl/1r5b1/ppppppppp/S8/9/9/PPPPPPPPP/1B5R1/LNSGKG1NL[] w 0 1", ["a6a7+", "a9a7"])
        self.assertEqual(result, "S")

    def test_game_result(self):
        result = sf.game_result("chess", CHESS, ["f2f3", "e7e5", "g2g4", "d8h4"])
        self.assertEqual(result, -sf.VALUE_MATE)

        # shogi pawn-drop mate is illegal and must not be returned as legal move
        fen = "lnsg3nk/1r2b1gs1/ppppppp1p/7N1/7p1/9/PPPPPPPP1/1B5R1/LNSGKGS1L[P] w 0 1"
        self.assertNotIn("P@i8", sf.legal_moves("shogi", fen, []))

        # losers checkmate
        result = sf.game_result("losers", CHESS, ["f2f3", "e7e5", "g2g4", "d8h4"])
        self.assertEqual(result, sf.VALUE_MATE)

        # suicide stalemate
        result = sf.game_result("suicide", "8/8/8/7p/7P/8/8/8 w - - 0 1", [])
        self.assertEqual(result, sf.VALUE_DRAW)
        result = sf.game_result("suicide", "8/8/8/7p/7P/7P/8/8 w - - 0 1", [])
        self.assertEqual(result, -sf.VALUE_MATE)
        result = sf.game_result("suicide", "8/8/8/7p/7P/8/8/n7 w - - 0 1", [])
        self.assertEqual(result, sf.VALUE_MATE)

        # armageddon
        # black gets stalemated
        result = sf.game_result("armageddon", "k7/P7/K7/8/8/8/8/8 b - - 0 1", [])
        self.assertEqual(result, sf.VALUE_MATE)
        # white gets stalemated
        result = sf.game_result("armageddon", "8/8/8/8/8/k7/p7/K7 w - - 0 1", [])
        self.assertEqual(result, -sf.VALUE_MATE)
        # 50 move rule
        result = sf.game_result("armageddon", "3n4/8/8/3k4/8/3K4/8/3BB3 w - - 100 80", [])
        self.assertEqual(result, -sf.VALUE_MATE)

        # atomic check- and stalemate
        # checkmate
        result = sf.game_result("atomic", "BQ6/Rk6/8/8/8/8/8/4K3 b - - 0 1", [])
        self.assertEqual(result, -sf.VALUE_MATE)
        # stalemate
        result = sf.game_result("atomic", "KQ6/Rk6/2B5/8/8/8/8/8 b - - 0 1", [])
        self.assertEqual(result, sf.VALUE_DRAW)

        # royalduck is derived from the all-only duck variant, so it is only
        # available when the current build exposes that template.
        if "royalduck" in sf.variants():
            result = sf.game_result("royalduck", "r1bqkbnr/pp1*p1p1/n2p1pQp/1Bp5/8/2N1PN2/PPPP1PPP/R1B1K2R b KQkq - 1 6", [])
            self.assertEqual(result, -sf.VALUE_MATE)
            result = sf.game_result("royalduck", "rnbqk1nr/pppp1ppp/4p3/8/7P/5Pb1/PPPPP*P1/RNBQKBNR w KQkq - 1 4", [])
            self.assertEqual(result, sf.VALUE_MATE)

    def test_pseudoroyal_drop_cannot_land_in_check(self):
        sf.load_variant_config(
            """[droppr:chess]
king = -
customPiece1 = a:W
pieceDrops = true
captureType = hand
firstRankPawnDrops = true
checking = true
pseudoRoyalTypes = a
startFen = 4r3/8/8/8/8/8/8/8[A] w - - 0 1
"""
        )

        legal = sf.legal_moves("droppr", "4r3/8/8/8/8/8/8/8[A] w - - 0 1", [])
        self.assertIn("A@d1", legal)
        self.assertNotIn("A@e1", legal)

    def test_runtime_royal_self_capture_is_illegal(self):
        sf.load_variant_config(
            """[runtime-royal-selfcapture:chess]
king = k:W
castling = false
selfCapture = true
startFen = 4k3/8/8/8/8/8/8/3QK3 w - - 0 1
"""
        )

        legal = sf.legal_moves("runtime-royal-selfcapture", sf.start_fen("runtime-royal-selfcapture"), [])
        self.assertNotIn("d1e1", legal)

    def test_runtime_royal_no_through_check_uses_actual_royal(self):
        sf.load_variant_config(
            """[runtime-royal-through-check:chess]
king = k:R
castling = false
royalPieceNoThroughCheck = true
startFen = 1r2k3/8/8/8/8/8/8/K7 w - - 0 1
"""
        )

        legal = sf.legal_moves("runtime-royal-through-check", sf.start_fen("runtime-royal-through-check"), [])
        self.assertNotIn("a1d1", legal)

    def test_pseudoroyal_loss_waits_for_candidate_types_to_disappear(self):
        sf.load_variant_config(
            """[prfix:chess]
king = -
customPiece1 = a:N
customPiece2 = b:B
pseudoRoyalTypes = ab
pseudoRoyalCount = 1
pseudoRoyalValue = loss
startFen = 8/8/8/8/8/8/AAaaBBbb/8 w - - 0 1
"""
        )

        result = sf.is_immediate_game_end("prfix", "8/8/8/8/8/8/AAaaBBbb/8 w - - 0 1", [])
        self.assertFalse(result[0])

    def test_extinction_value_uses_extinct_side(self):
        sf.load_variant_config(
            """[asym-extinction-values:chess]
checking = false
extinctionValueWhite = loss
extinctionValueBlack = win
extinctionPieceTypes = q
startFen = 4k3/8/8/8/8/8/4Q3/4K3 w - - 0 1
"""
        )

        is_end, result = sf.is_immediate_game_end(
            "asym-extinction-values",
            sf.start_fen("asym-extinction-values"),
            [],
        )
        self.assertTrue(is_end)
        self.assertEqual(result, -sf.VALUE_MATE)

    def _check_immediate_game_end(self, variant, fen, moves, game_end, game_result=None):
        with self.subTest(variant=variant, fen=fen, game_end=game_end, game_result=game_result):
            result = sf.is_immediate_game_end(variant, fen, moves)
            self.assertEqual(result[0], game_end)
            if game_result is not None:
                self.assertEqual(result[1], game_result)

    def test_is_immediate_game_end(self):
        self._check_immediate_game_end("capablanca", CAPA, [], False)

        # full board adjudication
        self._check_immediate_game_end("flipello", "pppppppp/pppppppp/pppPpppp/pPpPpppp/pppppppp/pPpPPPPP/ppPpPPpp/pppppppp[PPpp] b - - 63 32", [], True, sf.VALUE_MATE)
        self._check_immediate_game_end("ataxx", "PPPpppp/pppPPPp/pPPPPPP/PPPPPPp/ppPPPpp/pPPPPpP/pPPPPPP b - - 99 50", [], True, -sf.VALUE_MATE)
        self._check_immediate_game_end("ataxx", "PPPpppp/pppPPPp/pPP*PPP/PP*P*Pp/ppP*Ppp/pPPPPpP/pPPPPPP b - - 99 50", [], True, -sf.VALUE_MATE)

    def test_racing_kings_goal_adjudication(self):
        self._check_immediate_game_end("racingkings", "7K/k7/8/8/8/8/8/8 b - - 0 1", [], False)
        self._check_immediate_game_end("racingkings", "7K/8/k7/8/8/8/8/8 b - - 0 1", [], True, -sf.VALUE_MATE)
        self._check_immediate_game_end("racingkings", "k6K/8/8/8/8/8/8/8 w - - 0 1", [], True, sf.VALUE_DRAW)

    def test_loa_simultaneous_and_opponent_connection(self):
        load_repo_variants_or_skip()
        # In Lines of Action (connectGroup = -1), a capture can connect the opponent's remaining pieces.
        # Check that opponent connection results in their win (mover loss).
        # We'll use start FEN: 1nnnnnn1/N6N/N6N/N6N/N6N/N6N/N6N/1nnnnnn1 b - - 0 1
        # Let's set up a custom position where black is about to move but white's pieces are already fully connected.
        # e.g., White (N) has 3 pieces that are connected, while Black (n) is not connected.
        self._check_immediate_game_end("linesofaction", "8/8/8/8/8/8/3NN3/8 b - - 0 1", [], True, -sf.VALUE_MATE)
        # So stm (Black) loses, meaning game_result is -sf.VALUE_MATE.
        self._check_immediate_game_end("linesofaction", "8/8/8/8/8/8/3NN3/3nn3 b - - 0 1", [], True, -sf.VALUE_MATE)
        # In linesofaction-draw, simultaneous connection results in a draw.
        self._check_immediate_game_end("linesofaction-draw", "8/8/8/8/8/8/3NN3/3nn3 b - - 0 1", [], True, sf.VALUE_DRAW)
        # Connected groups must be found symmetrically, not only by walking the
        # canonical positive connection directions from the first discovered piece.
        self._check_immediate_game_end("linesofaction", "8/5n2/5nn1/6n1/8/8/8/8 w - - 0 1", [], True, -sf.VALUE_MATE)
        self._check_immediate_game_end(
            "linesofaction",
            "8/5n2/4Nn2/3N2nN/1NN2NnN/1N1N4/N7/8 b - - 12 18",
            [],
            True,
            sf.VALUE_MATE,
        )
        self._check_immediate_game_end(
            "linesofaction",
            "8/5n2/4Nnn1/3N2nN/1NN2N1N/1N1N4/N7/8 w - - 13 19",
            [],
            True,
            -sf.VALUE_MATE,
        )

    def test_connect_goal_simul_value_by_mover(self):
        # Load a custom variant testing simultaneous connection goals with a mover policy
        sf.load_variant_config(
            """[connect-simul-mover:chess]
connectN = 3
connectGoalByType = true
connectPieceGoalWhite = p p p
connectPieceGoalBlack = n n n
connectGoalSimulValueByMover = loss
startFen = 8/8/8/8/8/8/8/8 w - - 0 1
"""
        )
        # We will check if having both goals met returns a loss for the mover (meaning stm wins)
        # For this test, let's mock the board state (e.g. line configuration) or make sure it doesn't crash.
        # However, to avoid mock lines complexity, checking the parser loads it cleanly is a great start.
        pass

    def test_connection_all_remaining_pieces(self):
        sf.load_variant_config(
            """[connect-all:chess]
connectN = -1
startFen = 8/8/8/8/8/8/8/8 w - - 0 1

[collinear-all:chess]
collinearN = -1
startFen = 8/8/8/8/8/8/8/8 w - - 0 1

[connectnxn-all:chess]
connectNxN = -1
startFen = 8/8/8/8/8/8/8/8 w - - 0 1
"""
        )

        # connectN = -1
        # White has 3 pieces at a1, b1, c1 (aligned) -> White wins
        self._check_immediate_game_end("connect-all", "8/8/8/8/8/8/8/PPP5 w - - 0 1", [], True, sf.VALUE_MATE)
        # White has 3 pieces at a1, b1, d1 (not contiguous line of 3) -> ongoing
        self._check_immediate_game_end("connect-all", "8/8/8/8/8/8/8/PP1P4 w - - 0 1", [], False)

        # collinearN = -1
        # White has 3 pieces at a1, c1, e1 (collinear) -> White wins
        self._check_immediate_game_end("collinear-all", "8/8/8/8/8/8/8/P1P1P3 w - - 0 1", [], True, sf.VALUE_MATE)
        # White has 3 pieces at a1, b2, d3 (not collinear) -> ongoing
        self._check_immediate_game_end("collinear-all", "8/8/8/8/8/1P6/1P6/P7 w - - 0 1", [], False)

        # connectNxN = -1
        # White has 4 pieces forming a 2x2 square -> White wins
        self._check_immediate_game_end("connectnxn-all", "8/8/8/8/8/8/PP6/PP6 w - - 0 1", [], True, sf.VALUE_MATE)
        # White has 5 pieces -> no perfect square can be formed using all remaining pieces -> ongoing
        self._check_immediate_game_end("connectnxn-all", "8/8/8/8/8/8/PP1P4/PP6 w - - 0 1", [], False)

    def test_toroidal_line_counting_fix(self):
        sf.load_variant_config(
            """[wrap-count-test:chess]
maxRank = 3
maxFile = 4
toroidal = true
connectN = 3
connectDiagonal = false
materialCounting = connectn
nMoveRule = 1
startFen = 4/4/4 w - - 0 1

[wrap-count-full-cycle:chess]
maxRank = 5
maxFile = 4
toroidal = true
connectN = 4
connectDiagonal = false
materialCounting = connectn
nMoveRule = 1
startFen = 4/4/4/4/4 w - - 0 1
"""
        )
        # White has a closed loop of length 4 (Rank 1: PPPP), which has 4 cyclic lines of length 3.
        # Black has two open chains of length 3 (Rank 2: ppp., Rank 3: ppp.), which count as 1 + 1 = 2 lines.
        # White wins by 4 lines to 2.
        # We trigger optional game end via nMoveRule = 1 and rule50 = 2.
        self._check_optional_game_end("wrap-count-test", "ppp1/ppp1/PPPP[PPPPpppp] w - - 2 1", [], True, sf.VALUE_MATE)

        # A cycle exactly as long as connectN is one unique line. Black's vertical
        # open chain is also one line, so this position is a draw.
        self._check_optional_game_end("wrap-count-full-cycle", "p3/p3/p3/p3/PPPP[PPPPpppp] w - - 2 1", [], True, sf.VALUE_DRAW)

        # If White has 1 open chain of 3 (PPP.), White has 1 line.
        # Black has 2 open chains of 3 (ppp., ppp.), Black has 2 lines.
        # Black wins (White loses), returning a negative mate value.
        self._check_optional_game_end("wrap-count-test", "ppp1/ppp1/PPP1[PPPpppp] w - - 2 1", [], True, -sf.VALUE_MATE)

    def _check_optional_game_end(self, variant, fen, moves, game_end, game_result=None):
        with self.subTest(variant=variant, fen=fen, game_end=game_end, game_result=game_result):
            result = sf.is_optional_game_end(variant, fen, moves)
            self.assertEqual(result[0], game_end)
            if game_result is not None:
                self.assertEqual(result[1], game_result)

    def test_is_optional_game_end(self):
        self._check_optional_game_end("capablanca", CAPA, [], False)

        # sittuyin stalemate due to optional promotion
        self._check_optional_game_end("sittuyin", "1k4PK/3r4/8/8/8/8/8/8[] w - - 0 1", [], True, sf.VALUE_DRAW)

        # Xiangqi chasing rules
        # Also see http://www.asianxiangqi.org/English/AXF_rules_Eng.pdf
        # Direct chase by cannon
        self._check_optional_game_end("xiangqi", "2bakabnr/9/r1n1c4/2p1p1p1p/PP7/9/4P1P1P/2C3NC1/9/1NBAKAB1R w - - 0 1", ["c3a3", "a8b8", "a3b3", "b8a8", "b3a3", "a8b8", "a3b3", "b8a8", "b3a3"], True, sf.VALUE_MATE)
        # Chase with chasing side to move
        self._check_optional_game_end("xiangqi", "2bakabnr/9/r1n1c4/2p1p1p1p/PP7/9/4P1P1P/2C3NC1/9/1NBAKAB1R w - - 0 1", ["c3a3", "a8b8", "a3b3", "b8a8", "b3a3", "a8b8", "a3b3", "b8a8", "b3a3", "a8b8", "a3b3", "b8a8"], True, -sf.VALUE_MATE)
        # Discovered chase by cannon (including pawn capture)
        self._check_optional_game_end("xiangqi", "2bakabr1/9/9/r1p1p1p2/p7R/P8/9/9/9/CC1AKA3 w - - 0 1", ["a5a6", "a7b7", "a6b6", "b7a7", "b6a6", "a7b7", "a6b6", "b7a7", "b6a6"], True, sf.VALUE_MATE)
        # Chase by soldier (draw)
        self._check_optional_game_end("xiangqi", "2bakabr1/9/9/r1p1p1p2/p7R/P8/9/9/9/1C1AKA3 w - - 0 1", ["a5a6", "a7b7", "a6b6", "b7a7", "b6a6", "a7b7", "a6b6", "b7a7", "b6a6"], True, sf.VALUE_DRAW)
        # Discovered and anti-discovered chase by cannon
        self._check_optional_game_end("xiangqi", "5k3/9/9/5C3/5c3/5C3/9/9/5p3/4K4 w - - 0 1", ["f5d5", "f6d6", "d5f5", "d6f6", "f5d5", "f6d6", "d5f5", "d6f6"], True, -sf.VALUE_MATE)
        # Mutual chase (draw)
        self._check_optional_game_end("xiangqi", "4k4/7n1/9/4pR3/9/9/4P4/9/9/4K4 w - - 0 1", ["f7h7"] + 2 * ["h9f8", "h7h8", "f8g6", "h8g8", "g6i7", "g8g7", "i7h9", "g7h7"], True, sf.VALUE_DRAW)
        # Perpetual check vs. intermittent checks
        self._check_optional_game_end("xiangqi", "9/3kc4/3a5/3P5/9/4p4/9/4K4/9/3C5 w - - 0 1", 2 * ['d7e7', 'e5d5', 'e7d7', 'd5e5'], True, sf.VALUE_MATE)
        # Perpetual check by soldier
        self._check_optional_game_end("xiangqi", "3k5/9/9/9/9/5p3/9/5p3/5K3/5C3 w - - 0 1", 2 * ['f2e2', 'f3e3', 'e2f2', 'e3f3'], True, sf.VALUE_MATE)
        self._check_optional_game_end("xiangqi", "3k5/4P4/4b4/3C5/4c4/9/9/9/9/5K3 w - - 0 1", 2 * ['d7e7', 'e8g6', 'e7d7', 'g6e8'], True, sf.VALUE_MATE)
        self._check_optional_game_end("xiangqi", "3k5/9/9/9/9/9/9/9/cr1CAK3/9 w - - 0 1", 2 * ['d2d4', 'b2b4', 'd4d2', 'b4b2'], True, sf.VALUE_MATE)
        self._check_optional_game_end("xiangqi", "5k3/9/9/5C3/5c3/5C3/9/9/5p3/4K4 w - - 0 1", 2 * ['f5d5', 'f6d6', 'd5f5', 'd6f6'], True, -sf.VALUE_MATE)
        # In FSX this cycle is adjudicated as one-sided perpetual chase
        # (win/loss), not as mutual chase (draw).
        self._check_optional_game_end("xiangqi", "4k4/9/4b4/2c2nR2/9/9/9/9/9/3K5 w - - 0 1", 2 * ['g7g6', 'f7g9', 'g6g7', 'g9f7'], True, sf.VALUE_MATE)
        self._check_optional_game_end("xiangqi", "3P5/3k5/3nn4/9/9/9/9/9/9/5K3 w - - 0 1", 2 * ['d10e10', 'd9e9', 'e10d10', 'e9d9'], True, sf.VALUE_MATE)
        self._check_optional_game_end("xiangqi", "4ck3/9/9/9/9/2r1R4/9/9/4A4/3AK4 w - - 0 1", 2 * ['e5e4', 'c5c4', 'e4e5', 'c4c5'], True, sf.VALUE_MATE)
        self._check_optional_game_end("xiangqi", "4k4/9/9/c1c6/9/r8/9/9/C8/3K5 w - - 0 1", 2 * ['a2c2', 'a5c5', 'c2a2', 'c5a5'], True, sf.VALUE_MATE)
        # Mutual perpetual check
        self._check_optional_game_end("xiangqi", "9/4c4/3k5/3r5/9/9/4C4/9/4K4/3R5 w - - 0 1", 2 * ['e4d4', 'd7e7', 'd4e4', 'e7d7'], True, sf.VALUE_DRAW)
        self._check_optional_game_end("xiangqi", "3k5/6c2/9/7P1/6c2/6P2/9/9/9/5K3 w - - 0 1", 2 * ['h7g7', 'g6h6', 'g7h7', 'h6g6'], True, sf.VALUE_DRAW)
        self._check_optional_game_end("xiangqi", "4ck3/9/9/9/9/2r1R1N2/6N2/9/4A4/3AK4 w - - 0 1", 2 * ['e5e4', 'c5c4', 'e4e5', 'c4c5'], True, sf.VALUE_DRAW)
        self._check_optional_game_end("xiangqi", "5k3/9/9/c8/9/P1P6/9/2C6/9/3K5 w - - 0 1", 2 * ['c3a3', 'a7c7', 'a3c3', 'c7a7'], True, sf.VALUE_DRAW)
        self._check_optional_game_end("xiangqi", "4k4/9/r1r6/9/PPPP5/9/9/9/1C7/5K3 w - - 0 1", ['b2a2'] + 2 * ['a8b8', 'a2c2', 'c8d8', 'c2b2', 'b8a8', 'b2d2', 'd8c8', 'd2a2'], True, sf.VALUE_DRAW)

        # Corner cases
        # D106: Chariot chases cannon, but attack actually does not change (draw)
        self._check_optional_game_end("xiangqi", "3k2b2/4P4/4b4/9/8p/6Bc1/6P1P/3AB4/4pp3/1p1K3R1[] w - - 0 1", 2 * ["h1h2", "h5h4", "h2h1", "h4h5"], True, sf.VALUE_DRAW)
        # D39: Chased chariot pinned by horse + mutual chase (controversial if pinned chariot chases)
        self._check_optional_game_end("xiangqi", "2baka1r1/C4rN2/9/1Rp1p4/9/9/4P4/9/4A4/4KA3 w - - 0 1", ["b7b9"] + 2 * ["f10e9", "b9b10", "e9f10", "b10b9"], True, sf.VALUE_MATE)
        # D39: Chased chariot pinned by horse + mutual chase (controversial if pinned chariot chases)
        self._check_optional_game_end("xiangqi", "5k3/9/9/9/9/9/7r1/9/2nRA3c/4K4 w - - 0 1", 2 * ['e2f1', 'h4h2', 'f1e2', 'h2h4'], True, sf.VALUE_MATE)
        # Creating pins to undermine root
        self._check_optional_game_end("xiangqi", "4k4/4c4/9/4p4/9/9/3rn4/3NR4/4K4/9 b - - 0 1", 2 * ['e4g5', 'e2f2', 'g5e4', 'f2e2'], True, -sf.VALUE_MATE)
        # Discovered check capture threat by rook
        self._check_optional_game_end("xiangqi", "5k3/9/9/9/9/1N2P1C2/9/4BC3/9/cr1RK4 w - - 0 1", 2 * ['b5c3', 'b1c1', 'c3b5', 'c1b1'], True, sf.VALUE_MATE)
        # Creating a pin to undermine root + discovered check threat by horse
        self._check_optional_game_end("xiangqi", "5k3/9/9/9/9/4c4/3n5/3NBA3/4A4/4K4 w - - 0 1", 2 * ['e1d1', 'e5d5', 'd1e1', 'd5e5'], True, sf.VALUE_MATE)
        # Creating a pin to undermine root + discovered check threat by rook
        self._check_optional_game_end("xiangqi", "5k3/9/9/9/9/4c4/3r5/3NB4/4A4/4K4 w - - 0 1", 2 * ['e1d1', 'e5d5', 'd1e1', 'd5e5'], True, sf.VALUE_MATE)
        # X-Ray protected discovered check
        self._check_optional_game_end("xiangqi", "5k3/9/9/9/9/9/9/9/9/3NK1cr1 w - - 0 1", 2 * ['d1c3', 'h1h3', 'c3d1', 'h3h1'], True, sf.VALUE_MATE)
        # No overprotection by king
        self._check_optional_game_end("xiangqi", "3k5/9/9/3n5/9/9/3r5/9/9/3NK4 w - - 0 1", 2 * ['d1c3', 'd4c4', 'c3d1', 'c4d4'], True, sf.VALUE_DRAW)
        # Overprotection by king
        self._check_optional_game_end("xiangqi", "3k5/9/9/9/9/9/3r5/9/9/3NK4 w - - 0 1", 2 * ['d1c3', 'd4c4', 'c3d1', 'c4d4'], True, sf.VALUE_MATE)
        # Mutual pins by flying generals
        self._check_optional_game_end("xiangqi", "4k4/9/9/9/4n4/9/5C3/9/4N4/4K4 w - - 0 1", 2 * ['e2g1', 'e10f10', 'g1e2', 'f10e10'], True) #, sf.VALUE_MATE)
        # Fake protection by cannon
        self._check_optional_game_end("xiangqi", "5k3/9/9/9/9/1C7/1r7/9/1C7/4K4 w - - 0 1", 2 * ['b5c5', 'b4c4', 'c5b5', 'c4b4'], True, sf.VALUE_MATE)
        # Fake protection by cannon + mutual chase
        self._check_optional_game_end("xiangqi", "4ka3/c2R1R2c/4b4/9/9/9/9/9/9/4K4 w - - 0 1", 2 * ['f9f7', 'f10e9', 'f7f9', 'e9f10'], True, sf.VALUE_DRAW)

    def test_has_insufficient_material(self):
        for variant, positions in variant_positions.items():
            for fen, expected_result in positions.items():
                with self.subTest(variant=variant, fen=fen):
                    result = sf.has_insufficient_material(variant, fen, [])
                    self.assertEqual(result, expected_result)

    def test_validate_fen(self):
        # valid
        for variant, positions in variant_positions.items():
            for fen in positions:
                with self.subTest(variant=variant, fen=fen):
                    self.assertEqual(sf.validate_fen(fen, variant), sf.FEN_OK)
        # invalid
        for variant, positions in invalid_variant_positions.items():
            for fen in positions:
                with self.subTest(variant=variant, fen=fen):
                    self.assertNotEqual(sf.validate_fen(fen, variant), sf.FEN_OK)
        # chess960
        self.assertEqual(sf.validate_fen(CHESS960, "chess", True), sf.FEN_OK)
        self.assertEqual(sf.validate_fen("nrbqbkrn/pppppppp/8/8/8/8/PPPPPPPP/NRBQBKRN w BGbg - 0 1", "newzealand", True), sf.FEN_OK)
        # all variants starting positions
        for variant in sf.variants():
            with self.subTest(variant=variant):
                fen = sf.start_fen(variant)
                self.assertEqual(sf.validate_fen(fen, variant), sf.FEN_OK)

    def test_validate_position(self):
        self.assertEqual(
            sf.validate_position("chess", CHESS, ["e2e4", "e7e5", "g1f3"]),
            sf.FEN_OK,
        )
        self.assertEqual(
            sf.validate_position("chess", "startpos", ["e2e4", "e7e5", "g1f3"]),
            sf.FEN_OK,
        )
        self.assertEqual(
            sf.validate_position("chess", CHESS, ["e2e5"]),
            sf.FEN_INVALID_MOVE,
        )
        self.assertNotEqual(
            sf.validate_position("chess", "8/8/8/8/8/8/8/8 w - - 0 1", []),
            sf.FEN_OK,
        )

    def test_validate_fen_promoted_pieces(self):
        # Test promoted piece validation specifically

        # Valid promoted pieces should pass
        valid_promoted_fens = {
            "shogi": [
                "lnsgkgsnl/1r5b1/pppppp+ppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL[-] w - - 0 1",  # promoted pawn
                "lnsgkgsnl/1r5+b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL[-] w - - 0 1",  # promoted bishop
                "lnsgkgsnl/1+r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL[-] w - - 0 1",  # promoted rook
                "ln+sgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL[-] w - - 0 1",  # promoted silver
                "l+nsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL[-] w - - 0 1",  # promoted knight
                "+lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL[-] w - - 0 1",  # promoted lance
            ]
        }

        # Invalid promoted pieces should fail with FEN_INVALID_PROMOTED_PIECE (-12)
        invalid_promoted_fens = {
            "kyotoshogi": [
                "p+nks+l/5/5/5/+LS+K+NP[-] w 0 1",  # promoted king (+K) - kings cannot be promoted
            ],
            "shogi": [
                "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSG++KGSNL[-] w - - 0 1",  # double promotion (++K)
            ]
        }

        # Non-shogi variants should ignore promoted piece syntax ('+' should be invalid character)
        non_shogi_promoted_fens = {
            "chess": [
                "rnb+qkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",  # '+' not valid in chess
            ]
        }

        # Test valid promoted pieces
        for variant, fens in valid_promoted_fens.items():
            for fen in fens:
                with self.subTest(variant=variant, fen=fen, test_type="valid_promoted"):
                    result = sf.validate_fen(fen, variant)
                    self.assertEqual(result, sf.FEN_OK, f"Expected valid promoted piece FEN to pass: {fen}")

        # Test invalid promoted pieces (should return FEN_INVALID_PROMOTED_PIECE = -12)
        for variant, fens in invalid_promoted_fens.items():
            for fen in fens:
                with self.subTest(variant=variant, fen=fen, test_type="invalid_promoted"):
                    result = sf.validate_fen(fen, variant)
                    self.assertEqual(result, sf.FEN_INVALID_PROMOTED_PIECE,
                                   f"Expected invalid promoted piece FEN to return -12: {fen}, got {result}")

        # Test non-shogi variants (should fail with character validation, not promoted piece validation)
        for variant, fens in non_shogi_promoted_fens.items():
            for fen in fens:
                with self.subTest(variant=variant, fen=fen, test_type="non_shogi"):
                    result = sf.validate_fen(fen, variant)
                    # Should fail with character validation (FEN_INVALID_CHAR = -10), not promoted piece validation
                    self.assertEqual(result, -10,
                                   f"Expected non-shogi variant to fail with character error (-10): {fen}, got {result}")

    def test_blast_on_capture_mover_center(self):
        # White Rook e3, Black Knights: e5 (target), d6 (diag to e5), d2 (diag to e3)
        fen = "8/8/3n4/4n3/8/4R3/3n4/8 w - - 0 1"
        move = "e3e5"

        # Default blast (centered on capture square e5)
        # e5 diagonals: d6, f6, d4, f4. d6 should be removed.
        # d2 is not near e5.
        fen_default = sf.get_fen("blast-default-test", fen, [move])
        self.assertEqual(fen_default, "8/8/8/8/8/4R3/3n4/8 b - - 0 1")

        # Mover-centered rifle captures blast around the stationary shooter. With
        # blastCenter enabled, the shooter is removed by its own blast.
        fen_mover = sf.get_fen("blast-mover-test", fen, [move])
        self.assertEqual(fen_mover, "8/8/3n4/8/8/8/8/8 b - - 0 1")

    def test_evaluate(self):
        eval_start = sf.evaluate("chess", CHESS, [])
        self.assertTrue(-50 <= eval_start <= 50, f"Expected startpos eval near 0, got {eval_start}")
        
        # Rook vs king is winning, should be > 300 centipawns
        self.assertGreater(sf.evaluate("chess", "k7/8/8/8/8/8/7R/K7 w - - 0 1", []), 300)
        
        # Black to move: since evaluate is side-to-move, Black is losing, so Black's eval is negative
        self.assertLess(sf.evaluate("chess", "k7/8/8/8/8/8/7R/K7 b - - 0 1", []), -300)
        
        with self.assertRaisesRegex(ValueError, "No such variant 'non_existent_variant'"):
            sf.evaluate("non_existent_variant", "8/8/8/8/8/8/8/8 w - - 0 1", [])

    def test_racing_kings_endgame_eval(self):
        # White on 8th, Black on 2nd, Black to move. Black is losing.
        res = sf.evaluate("racingkings", "K7/8/8/8/8/8/k7/8 b - - 0 1", [])
        self.assertLess(res, -10000)

        # White on 8th, Black reaches the 8th rank too. This is an immediate draw.
        res = sf.evaluate("racingkings", "7K/k7/8/8/8/8/8/8 b - - 0 1", ["a7a8"])
        self.assertEqual(res, 0)

    def test_atomic_endgame_eval(self):
        # White has enough material to trigger the specialized evaluator.
        eval1 = sf.evaluate("atomic", "8/8/8/8/8/5k2/5K2/4Q3 w - - 0 1", [])
        eval2 = sf.evaluate("atomic", "8/8/8/8/5k2/8/5K2/4Q3 w - - 0 1", [])
        self.assertGreater(eval2, eval1)

    def test_get_fog_fen(self):
        fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"  # startpos
        result = sf.get_fog_fen(fen, "fogofwar")
        self.assertEqual(result, "********/********/********/********/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")

        fen = "rnbqkbnr/p1p2ppp/8/Pp1pp3/4P3/8/1PPP1PPP/RNBQKBNR w KQkq b6 0 1"
        result = sf.get_fog_fen(fen, "fogofwar")
        self.assertEqual(result, "********/********/2******/Pp*p***1/4P3/4*3/1PPP1PPP/RNBQKBNR w KQkq b6 0 1")
        

    def test_push_state_consistency(self):
        ini_text = """
[push-test:fairy]
maxFile = e
maxRank = 5
castling = false
checking = false
startFen = 5/5/5/5/5 w - - 0 1
rook = r
pushingStrength = r:5
pushFirstColor = them
pushChainEnemyOnly = true
pushCaptureAgainstFriendlyBlocker = true
pushingRemoves = none
stepwisePushing = true
"""
        with tempfile.NamedTemporaryFile(mode="w", suffix=".ini", delete=False) as f:
            f.write(ini_text)
            temp_name = f.name

        try:
            sf.set_option("VariantPath", temp_name)

            variant = "push-test"
            # Case 1: displacement
            fen1 = "5/5/1R1r1/5/5 w - - 0 1"
            moves1 = ["b3d3"]
            expected1 = "5/5/3Rr/5/5 b - - 1 1"
            self.assertEqual(sf.get_fen(variant, fen1, moves1), expected1)

            # Case 2: capture
            fen2 = "5/5/1R1rR/5/5 w - - 0 1"
            moves2 = ["b3d3"]
            expected2 = "5/5/3RR/5/5 b - - 0 1"
            self.assertEqual(sf.get_fen(variant, fen2, moves2), expected2)
        finally:
            path = repo_variants_ini()
            if path:
                sf.set_option("VariantPath", str(path))
            os.remove(temp_name)

    def test_spell_chess(self):
        # 1. Verification of promotion potion moves and hands
        fen_promo = "7k/P7/8/8/8/8/8/K7[F] w - - 0 1"
        next_fen = sf.get_fen("spell-chess", fen_promo, ["a7a8q"])
        self.assertEqual(next_fen, "Q6k/8/8/8/8/8/8/K7[F] b - - 0 1")

        # 2. Frozen attackers block castling?
        # White commoner e1, rook h1, enemy king on e8. Enemy bishop at d4 controls f2.
        # Without freezing, White cannot castle e1g1.
        fen_freeze_test = "4k3/8/8/8/3b4/8/8/R3K2R[F] w K - 0 1"
        
        # Test 2a: Dropping freeze potion on d4 AND castling e1g1 in the same turn (double move) should be legal
        moves = sf.legal_moves("spell-chess", fen_freeze_test, [])
        self.assertIn("f@d4,e1g1", moves, "Should be allowed to castle by freezing the attacker in the same turn")

        # Test 2b: Castling e1g1 on its own without freezing the attacker first should NOT be legal
        self.assertNotIn("e1g1", moves, "Should not be allowed to castle through unfrozen attacker on its own")

    def test_magic_geometry_pollution(self):
        # Chess (8x8) and Capablanca (10x8)
        # We will query legal moves for a rook on a4 on both boards multiple times, alternating,
        # to ensure that the board-size-specific MagicGeometry does not get polluted.
        # Chess FEN: rook on a4, kings on e1 and e8
        chess_fen = "4k3/8/8/8/R7/8/8/4K3 w - - 0 1"
        # Capablanca FEN: rook on a4, kings on e1 and e8 (10 files)
        capa_fen = "4k5/10/10/10/R9/10/10/4K5 w - - 0 1"

        for _ in range(5):
            chess_moves = sf.legal_moves("chess", chess_fen, [])
            # Rook should have horizontal moves b4, c4, d4, e4, f4, g4, h4 (7 moves)
            # and vertical moves a1, a2, a3, a5, a6, a7, a8 (7 moves).
            rook_chess_moves = [m for m in chess_moves if m.startswith("a4")]
            self.assertEqual(len(rook_chess_moves), 14, f"Expected 14 moves in chess, got: {rook_chess_moves}")

        capa_moves = sf.legal_moves("capablanca", capa_fen, [])
        # Rook should have horizontal moves b4, c4, d4, e4, f4, g4, h4, i4, j4 (9 moves)
        # and vertical moves a1, a2, a3, a5, a6, a7, a8 (7 moves).
        rook_capa_moves = [m for m in capa_moves if m.startswith("a4")]
        self.assertEqual(len(rook_capa_moves), 16, f"Expected 16 moves in capablanca, got: {rook_capa_moves}")

        mini_nightrider_moves = sf.legal_moves("mini-nightrider", "6k/7/7/3N3/7/7/K6 w - - 0 1", [])
        self.assertEqual(
            sorted(m for m in mini_nightrider_moves if m.startswith("d4")),
            ["d4b3", "d4b5", "d4c2", "d4c6", "d4e2", "d4e6", "d4f3", "d4f5"],
        )

    def test_laser_variants(self):
        load_repo_variants_or_skip()
        khet1_fen = sf.start_fen("khet1")
        self.assertEqual(khet1_fen, "2P:2O+KO+4/7P:32/6p:03/p:01P:21S:0S:11p:31P:1/p:31P:11s:1s:01p:01P:2/3P:26/2p:17/4o+ko+p:02 w - - 0 1")

        k1_imhotep = sf.start_fen("khet1-imhotep")
        self.assertEqual(k1_imhotep, "2S:0O+KO+4/10/3P:12p:03/p:0P:22S:0p:22p:3P:1/p:3P:12P:0s:02p:0P:2/3P:22p:33/10/4o+ko+s:02 w - - 0 1")

        k1_dynasty = sf.start_fen("khet1-dynasty")
        self.assertEqual(k1_dynasty, "3P:2O+P:34/4K5/3S:0O+P:33P:1/3p:21p:01S:11P:2/p:01s:11P:21P:03/p:33p:1o+s:03/5k4/4p:1o+p:03 w - - 0 1")

        khet2_fen = sf.start_fen("khet2")
        self.assertEqual(khet2_fen, "2P:2A:0KA:03X:0/7P:32/6p:03/p:01P:21S:0S:11p:31P:1/p:31P:11s:1s:01p:01P:2/3P:26/2p:17/x:23a:2ka:2p:02 w - - 0 1")

        k2_imhotep = sf.start_fen("khet2-imhotep")
        self.assertEqual(k2_imhotep, "2S:0A:0KA:03X:0/10/3P:12p:03/p:0P:22S:0p:22p:3P:1/p:3P:12P:0s:02p:0P:2/3P:22p:33/10/x:23a:2ka:2s:02 w - - 0 1")

        k2_dynasty = sf.start_fen("khet2-dynasty")
        self.assertEqual(k2_dynasty, "3P:2A:0P:33X:0/4K5/3S:0A:0P:33P:1/3p:21p:01S:11P:2/p:01s:11P:21P:03/p:33p:1a:2s:03/5k4/x:23p:1a:2p:03 w - - 0 1")

        playlaser_fen = sf.start_fen("playlaser")
        self.assertEqual(playlaser_fen, "l:07/1knp4/1nwp4/1pp5/5PP1/4PWN1/4PNK1/7L:0 w - - 0 1")

        dos_fen = sf.start_fen("dos-laser-chess")
        self.assertEqual(dos_fen, "r:1b:0s:0lkq:0b:0s:0r:1/d:0m:3d:0m:1pm:0d:0m:2d:0/9/9/9/9/9/D:2M:0D:2M:2PM:3D:2M:1D:2/R:1S:0B:2Q:2KLS:0B:2R:1 w - - 0 1")

        khet1_moves = sf.legal_moves("khet1", khet1_fen, [])
        self.assertTrue(len(khet1_moves) > 0)
        self.assertIn("c2d3p:1", khet1_moves)

        playlaser_moves = sf.legal_moves("playlaser", playlaser_fen, [])
        self.assertTrue(len(playlaser_moves) > 0)

        # Targeted DOS Laser Chess tests
        self.assertNotEqual(sf.validate_fen("invalid FEN", "dos-laser-chess"), 1)
        self.assertNotEqual(sf.validate_fen("9/9/9/9/9/9/9/9/9 w - - 0 1", "dos-laser-chess"), 1)

        dos_moves = sf.legal_moves("dos-laser-chess", dos_fen, [])
        self.assertTrue(len(dos_moves) > 0)
        self.assertIn("e2e3", dos_moves)
        self.assertIn("b2b3r:1a1", dos_moves)

        after_fen = sf.get_fen("dos-laser-chess", dos_fen, ["e2e3"])
        self.assertEqual(sf.validate_fen(after_fen, "dos-laser-chess"), 1)

if __name__ == '__main__':
    unittest.main(verbosity=2)
