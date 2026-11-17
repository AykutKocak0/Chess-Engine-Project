#pragma once

// All tunable evaluation weights, kept in one place so that a tuner (for example
// Texel tuning) can read and change them. Every member is an int, so the whole
// struct can also be walked as a flat int array (see paramData / PARAM_COUNT).
//
// Conventions:
//  - Values are from the point of view of the side that has the feature:
//    bonuses are positive, penalties are stored as negative numbers.
//  - EvalPair holds a middlegame and an endgame value; the evaluation blends
//    them by game phase.
//  - If values are changed at runtime, clear the pawn hash table (PTT.clear())
//    afterwards: it caches pawn-structure scores computed with the old values.
//
// Left out on purpose: the mop-up / KBN mating logic, draw detection and the
// phase thresholds. They encode endgame technique and structure rather than
// weights, and tuning them on game results tends to hurt mating ability.
//
// The values below come from two rounds of Texel tuning (Tuner.cpp) on 1.29M
// quiet positions from 90k self-play games (datagen, 5000 nodes per move).
// Round 1 (5 iterations) started from hand-set values and the PeSTO tables with
// all material fixed; self-play at 10+0.1 against the old values: +113 +/- 39 Elo.
// Round 2 (4 iterations) added the newer terms (imbalance, outposts, rook files,
// connected/backward pawns, passed pawn king distance and path, pawn storm,
// safe checks, attacker count, endgame scaling) with hand-set start values and
// tuned everything except pawn material and kingSafetyTable[55+] (reached too
// rarely to tune). Round 2 has not been tested in games yet.
// Hand corrections after tuning: kingSafetyTable was made non-decreasing, and a
// separate 0-safe-squares queen term was merged into queenFewMoves (it fired in
// almost no training positions, so tuning only fitted noise).
// Some terms overlap (for example passedPawnMg with the pawn PST on the 7th
// rank, connectedPawnMg with passedPawnMg there, centerPawn with centerPawnDuo
// and the d4/e4 squares), so a single value can look odd while the sum is sound.

struct EvalPair {
    int mg;
    int eg;
};

struct EvalParams {
    // Bonus for the side to move.
    int tempo = 19;

    // Material, indexed pawn, knight, bishop, rook, queen.
    EvalPair material[5] = { {100, 125}, {306, 302}, {332, 358}, {471, 639}, {1123, 1062} };
    EvalPair bishopPair = {32, 63};

    // Material imbalance (after Kaufman): knights gain and rooks lose value as
    // their own side keeps more pawns. Applied per piece, per own pawn above five.
    EvalPair knightPawnAdjust = {2, 11};
    EvalPair rookPawnAdjust   = {-6, -5};
    // Extra minor pieces against an extra rook, e.g. rook + two minors against
    // two rooks. Applied once per minor-for-rook difference, to the minors' side.
    EvalPair minorsForRook    = {-6, -25};

    // Pawn structure.
    EvalPair centerPawn          = {-13, -29};   // pawn on d4/e4 (d5/e5 for Black)
    EvalPair centerPawnDuo       = {9, 25};    // both center pawns
    EvalPair doubledPawn         = {-17, -31};  // per file holding 2+ own pawns
    EvalPair isolatedPawn        = {6, -15};     // per isolated pawn
    EvalPair protectedPassedPawn = {5, 9};      // passed pawn defended by a pawn
    // Passed pawn bonus by relative rank (0 = own back rank). A candidate passer
    // gets half of the bonus for the rank in front of it.
    int passedPawnMg[8] = { 0, 0, -20, -13, 0, 11, -39, 0 };
    int passedPawnEg[8] = { 0, 3, 24, 40, 76, 123, 119, 0 };
    // Pawn defended by an own pawn or standing next to one, by relative rank.
    int connectedPawnMg[8] = { 0, 4, 3, 5, 14, 38, 294, 0 };
    int connectedPawnEg[8] = { 0, -7, 5, 1, 14, 45, 70, 0 };
    // Backward pawn: no own pawn on an adjacent file can still defend it and an
    // enemy pawn controls the square in front. Extra when no enemy pawn blocks the
    // file, so enemy rooks can attack it.
    EvalPair backwardPawn     = {4, -13};
    EvalPair backwardOpenFile = {-16, -11};

    // Passed pawns, part that depends on pieces (not cached in the pawn hash).
    // By relative rank; endgame only. King distances are measured to the square
    // in front of the pawn.
    int passedOwnKingDist[8]   = { 0, -1, -2, -6, -10, -12, -14, 0 };   // per square
    int passedEnemyKingDist[8] = { 0, 0, 0, 10, 23, 31, 36, 0 };    // per square
    int passedBlocked[8]       = { 0, -5, 6, -14, -34, -77, -112, 0 }; // square in front occupied
    int passedFreePath[8]      = { 0, 29, 1, 36, 54, 96, 113, 0 };    // way to promotion empty and not attacked
    // Pawn endgame: the enemy king can no longer catch the pawn (rule of the square).
    int unstoppablePasser = 277;

    // Piece-square tables [piece][square], piece order pawn..king. Square 0 is a8
    // (ChessProgramming wiki layout): White looks up sq ^ 56, Black looks up sq.
    int pstMg[6][64] = { { 0, 0, 0, 0, 0, 0, 0, 0, 145, 185, 176, 230, 183, 128, 29, 55, -2, -36, 3, 39, 66, 24, 1, -11, -34, -12, -6, 9, 13, -16, -9, -15, -38, -26, -11, 25, 14, -3, -16, -38, -25, -22, -14, -13, -14, -18, 21, -29, -46, -6, -32, -11, -15, 1, 31, -42, 0, 0, 0, 0, 0, 0, 0, 0 }, { -102, -110, -92, -85, 89, -130, -28, -42, 29, -17, 26, 17, 19, 67, -104, -30, 64, 75, 22, 78, 85, 91, 36, 122, 13, 24, 29, 52, 46, 69, 23, 46, 0, -29, 36, 15, 31, 31, 13, 2, -19, 25, 9, 18, 41, 29, 52, -3, 7, -28, -4, 30, 37, 23, 20, -7, -27, -12, -15, 24, 17, 55, 5, -89 }, { 8, -38, -76, -78, -16, -198, -123, -9, -60, -19, -26, -8, -20, -37, -44, 11, 5, 43, 22, 36, 40, 82, 30, 11, 5, 4, -5, 42, 28, 41, 13, 12, 15, -30, 4, 44, 27, 21, -13, -29, 11, 43, 20, 1, 11, 26, 37, 3, -24, 25, 8, 14, 12, 23, 38, 17, -72, -28, -3, -1, -4, -11, -91, 2 }, { 25, 81, 15, 33, 13, 18, 31, 36, -9, -5, -10, 48, 47, 49, 7, 21, -28, 30, -12, 30, 40, 39, 37, 26, -72, -28, -55, 12, -15, 30, -40, -64, -44, -60, -18, -25, -28, -36, -31, -35, -39, -28, -42, -35, -25, -18, -9, -33, -72, -13, -14, -37, -6, -23, 3, -93, -32, -33, -13, -1, 6, -3, -47, -35 }, { -57, -12, -6, 49, 81, 103, 45, -25, -11, -8, -8, 29, 11, 21, -15, 7, -13, 1, -6, 1, -18, 28, -6, 44, -27, -19, -22, -45, -58, -42, -37, -10, 9, -16, 8, -4, -25, 5, -1, 9, 18, 22, 5, 17, 12, 5, 35, 18, 37, 11, 30, 48, 52, 48, 64, 19, 19, -44, 42, 36, 47, 13, 84, -53 }, { -15, 91, 25, 3, -54, 7, 27, 18, 39, 13, -11, 14, -47, 4, -4, -27, 2, 23, -4, -6, -19, -84, -130, -1, -54, -26, -158, -43, -69, -31, -9, -67, -107, -79, -75, -41, -98, -120, -88, -125, -83, -67, -174, -119, -90, -110, -28, -43, 18, -33, -22, -51, -45, -24, 35, 27, -32, 33, -8, -49, 11, -48, 30, 15 } };
    int pstEg[6][64] = { { 0, 0, 0, 0, 0, 0, 0, 0, 93, 107, 124, 99, 108, 141, 161, 157, 39, 50, 32, -1, -22, 4, 11, 48, 15, 24, -16, -31, -31, 2, -3, 7, 13, 4, -21, -7, -16, -21, 12, -1, -2, -7, -18, -15, -19, -7, -17, -17, 10, 2, -3, -18, 1, -7, -18, -10, 0, 0, 0, 0, 0, 0, 0, 0 }, { -55, 28, -44, -6, -27, -4, -92, -46, -10, 15, -19, 28, 14, -19, 23, -42, 36, -25, 20, 17, 14, -8, 18, -48, 23, 45, 43, 34, 42, 30, 24, 5, 20, 13, 34, 54, 40, 28, 11, -40, -9, -8, 15, 21, 17, 10, -13, -4, 7, 7, -6, 12, 4, 3, -24, -33, -46, 13, -12, -18, 5, -17, -10, -23 }, { -4, -27, -10, 18, -21, 10, -12, -33, -5, -27, 1, -23, 19, 4, -1, -78, -10, -16, -7, -21, -13, -16, 4, -8, -27, 9, 19, 23, 4, 15, -3, -32, -40, 22, 16, 10, 15, -10, 18, -13, -1, -1, -10, 9, 17, -1, -27, -7, -17, -24, -16, -25, -20, -22, -35, -47, 20, -17, 6, -25, -23, 6, -4, -15 }, { 41, 13, 22, 4, 12, 38, 10, 30, 7, 5, -2, -21, -18, -13, 8, -20, 29, 4, 14, 3, -11, 19, 0, -4, 45, 23, 35, -4, 6, -5, 9, 23, 28, 9, 11, 1, 2, 21, 4, -16, 7, 1, -4, 0, -6, -7, -29, -4, 22, -7, 0, -1, -41, -21, -19, -11, -9, 2, -16, -19, -25, -21, 12, -18 }, { 109, 116, 140, 154, 131, 58, 103, 117, 108, 165, 177, 126, 193, 148, 158, 144, 77, 119, 188, 183, 199, 173, 144, 22, 165, 209, 174, 247, 240, 206, 199, 119, 28, 183, 158, 174, 195, 133, 116, 81, 70, 64, 108, 102, 130, 166, 47, 7, 67, 38, 35, 25, -3, 25, -56, -28, 54, 54, -14, -28, -5, 48, -15, 26 }, { 146, 2, 53, 58, -37, 27, 16, -14, -5, 68, 103, 76, 37, 52, 48, 33, 48, 55, 29, 60, 62, 38, 47, 43, 40, 66, 66, 35, 48, 54, 46, 31, 4, 21, 44, 50, 46, 65, 39, 8, 31, 33, 66, 45, 50, 47, 21, -4, -25, 10, 13, 27, 21, 17, -5, -17, -54, -45, -11, -6, -40, -14, -40, -58 } };

    // Mobility, indexed by the number of reachable squares that are neither
    // occupied by own pieces nor attacked by enemy pawns.
    int knightMobilityMg[9]  = { -27, -9, -6, -1, 6, 12, 20, 30, 44 };
    int knightMobilityEg[9]  = { -84, -22, 5, 18, 31, 41, 47, 49, 24 };
    int bishopMobilityMg[14] = { -43, -16, -2, 6, 11, 12, 11, 12, 21, 17, 37, 39, 14, 24 };
    int bishopMobilityEg[14] = { -93, -13, -13, 2, 14, 35, 40, 38, 39, 35, 34, 34, 37, 46 };
    int rookMobilityMg[15]   = { -54, -35, -24, -14, -9, 2, 2, 9, -3, 8, 17, 13, 8, -4, 11 };
    int rookMobilityEg[15]   = { -79, -37, -30, -12, -6, -4, 7, 10, 28, 25, 29, 27, 34, 28, 23 };
    // Queen that has left its back rank and can barely move.
    EvalPair queenFewMoves = {-74, -40};    // 0-2 safe squares

    // Outpost: knight or bishop on relative rank 4-6, defended by an own pawn and
    // out of reach of enemy pawns (none left on the adjacent files ahead of it).
    EvalPair knightOutpost = {29, 17};
    EvalPair bishopOutpost = {51, 17};

    // Rooks: file with no own pawn (open: no pawns at all), and the 7th rank when
    // enemy pawns are there or the enemy king is stuck on the 8th.
    EvalPair rookOpenFile     = {36, -4};
    EvalPair rookSemiOpenFile = {15, 17};
    EvalPair rookOn7th        = {12, 32};

    // King safety: each attacked square in the enemy king zone adds the
    // attacker's weight to a danger count, which indexes kingSafetyTable.
    int kingAttackKnight = 2;
    int kingAttackBishop = 2;
    int kingAttackRook   = 3;
    int kingAttackQueen  = 4;
    // Danger for each square from which an enemy piece could give check without
    // being captured (not attacked by us, not occupied by its own pieces).
    int safeCheckKnight = 9;
    int safeCheckBishop = 3;
    int safeCheckRook   = 15;
    int safeCheckQueen  = 7;
    // The king zone danger is scaled (in percent) by the number of enemy pieces
    // attacking the zone (0..4+), and again when the attacker has no queen.
    int kingAttackerScale[5] = { 50, 73, 92, 100, 100 };
    int kingDangerNoQueen = 69;
    int kingSafetyTable[100] = { 0, 0, 0, 3, 4, 4, 4, 10, 11, 16, 17, 17, 28, 31, 36, 46, 46, 48, 48, 62, 62, 86, 86, 87, 88, 108, 108, 128, 128, 128, 161, 174, 174, 183, 193, 223, 223, 247, 247, 251, 256, 261, 297, 297, 326, 326, 326, 326, 326, 343, 383, 404, 420, 420, 421, 436, 448, 459, 471, 483, 494, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500 };

    // King shelter (middlegame only).
    int kingFileNoOwnPawn     = -14;  // king on a flank file with no own pawn on it
    int kingFileFullyOpen     = -66;  // ...and no enemy pawn either (extra)
    int kingAdjFilesNoOwnPawn = -30;  // no own pawn on the files next to the king
    int kingCenterOpenFile    = -68;  // king on a center file with no pawns at all
    int shieldPawnUnmoved     = 20;   // shield pawn on its starting rank
    int shieldPawnAdvanced    = 10;    // shield pawn one step forward
    int shieldPawnMissing     = -12;  // neither
    // Pawn storm: the most advanced enemy pawn on the king's file and the files
    // next to it, by its rank seen from the defender (2 = on our 3rd rank).
    int pawnStorm[8]        = { 0, 2, -38, -11, -6, 8, 8, 0 };
    int pawnStormBlocked[8] = { 0, 0, -48, 8, 5, 3, -31, 0 };   // own pawn right in front of it

    // Trapped pieces (halved deep in the endgame).
    int trappedBishop7th = -161;  // e.g. white bishop a7 with black pawn b6
    int trappedBishop6th = -18;   // e.g. white bishop a6 with black pawn b7
    int knightInCorner   = -21;   // knight on the enemy's a8/h8 corner
    int trappedKnight7th = -87;   // knight on a7 next to enemy pawns on b6/c6

    // A piece pinned to its own king by a rook, bishop or queen.
    EvalPair pinnedPiece = {-11, -35};

    // Threats: a pawn or minor piece attacking a more valuable enemy piece.
    EvalPair pawnThreatQueen  = {60, 42};
    EvalPair pawnThreatRook   = {118, 25};
    EvalPair pawnThreatMinor  = {74, 60};
    EvalPair minorThreatQueen = {54, 54};
    EvalPair minorThreatRook  = {69, 55};

    // Endgame scaling, out of 64, applied to the endgame part of the score.
    int scaleOppositeBishops       = 29;  // only opposite-coloured bishops and pawns left
    int scaleOppositeBishopsPieces = 67;  // opposite bishops plus other pieces
    int scaleNoPawns               = 0;  // stronger side has no pawns and is at most a minor up
};

extern EvalParams evalParams;

// Flat view of all parameters, for tuners.
static_assert(sizeof(EvalParams) % sizeof(int) == 0, "EvalParams must contain only ints");
constexpr int PARAM_COUNT = sizeof(EvalParams) / sizeof(int);
inline int* paramData(EvalParams& p) { return reinterpret_cast<int*>(&p); }

#include <string>

// Name of the field that flat parameter index i belongs to, e.g. "pstMg[1][27]".
std::string paramName(int index);
// Checks that the field list in EvalParams.cpp covers every member exactly once.
bool paramFieldsComplete();

// Text format, one field per line: "<name> <values...>". Fields missing from the
// file keep their current values. Returns false (and prints why) on errors.
bool loadParams(const std::string& path, EvalParams& p);
bool saveParams(const std::string& path, const EvalParams& p);
// Writes the values as C++ member initializers, ready to paste into EvalParams.h.
bool saveParamsCpp(const std::string& path, const EvalParams& p);
