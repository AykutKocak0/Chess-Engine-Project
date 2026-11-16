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
// The values below come from Texel tuning (Tuner.cpp, 5 iterations) on 1.29M
// quiet positions from 90k self-play games (datagen, 5000 nodes per move),
// starting from hand-set values and the PeSTO tables. Material was kept fixed.
// Hand corrections after tuning: kingSafetyTable was made non-decreasing and
// indices 55+ were reset to the old values (they are reached too rarely to tune),
// and queenNoMoves.mg was reset to -150. Some terms overlap (for example
// passedPawnMg with the pawn PST on the 7th rank, centerPawn with centerPawnDuo
// and the d4/e4 squares), so a single value can look odd while the sum is sound.
// Self-play at 10+0.1 against the old values: +113 +/- 39 Elo.

struct EvalPair {
    int mg;
    int eg;
};

struct EvalParams {
    // Bonus for the side to move.
    int tempo = 18;

    // Material, indexed pawn, knight, bishop, rook, queen.
    EvalPair material[5] = { {100, 125}, {320, 285}, {330, 350}, {500, 600}, {900, 1000} };
    EvalPair bishopPair = {44, 50};

    // Pawn structure.
    EvalPair centerPawn          = {-2, -32};   // pawn on d4/e4 (d5/e5 for Black)
    EvalPair centerPawnDuo       = {22, 15};    // both center pawns
    EvalPair doubledPawn         = {-17, -36};  // per file holding 2+ own pawns
    EvalPair isolatedPawn        = {6, -8};     // per isolated pawn
    EvalPair protectedPassedPawn = {2, 8};      // passed pawn defended by a pawn
    // Passed pawn bonus by relative rank (0 = own back rank). A candidate passer
    // gets half of the bonus for the rank in front of it.
    int passedPawnMg[8] = { 0, -12, -28, -19, -8, 11, -67, 0 };
    int passedPawnEg[8] = { 0, 3, 18, 51, 91, 133, 152, 0 };

    // Piece-square tables [piece][square], piece order pawn..king. Square 0 is a8
    // (ChessProgramming wiki layout): White looks up sq ^ 56, Black looks up sq.
    int pstMg[6][64] = {
        { // pawn
               0,    0,    0,    0,    0,    0,    0,    0,
             140,  184,  159,  172,   98,  125,   28,   46,
              -2,  -16,    3,   39,   58,   32,   -2,    5,
             -34,   -6,   -1,   22,   25,    4,   -2,  -15,
             -38,  -22,   -8,   21,   12,    6,  -14,  -35,
             -23,  -17,   -7,  -12,  -12,  -17,   25,  -27,
             -50,   -1,  -31,  -13,  -18,    3,   37,  -46,
               0,    0,    0,    0,    0,    0,    0,    0,
        },
        { // knight
            -107, -110,  -94,  -68,   71, -129,  -25,  -49,
              12,  -18,   30,   23,   19,   72,  -99,  -28,
              67,   82,   30,   91,   99,  110,   43,  113,
              23,   29,   37,   68,   54,   79,   25,   51,
               4,  -21,   42,   16,   33,   33,    9,    6,
             -22,   19,   12,   18,   44,   28,   51,   -6,
               4,  -29,   -5,   28,   32,   24,   16,  -15,
             -28,  -19,  -33,   19,   10,   36,   -2,  -85,
        },
        { // bishop
             -13,  -32,  -77,  -70,  -21, -198, -123,  -30,
             -64,  -21,  -33,  -11,  -13,  -38,  -49,    8,
               2,   46,   28,   40,   40,   77,   32,    7,
              -1,    4,    0,   50,   28,   41,   13,    7,
               5,  -26,    5,   43,   27,   20,  -12,  -31,
               8,   38,   16,    1,    8,   24,   31,    4,
             -27,   22,    4,   14,   10,   21,   40,   14,
             -70,  -36,   -8,  -19,  -11,  -16,  -91,   -5,
        },
        { // rook
              32,   82,   14,   34,   13,   17,   28,   42,
              44,   40,   40,   84,   86,   77,   50,   50,
             -14,   42,    2,   38,   41,   44,   46,   34,
             -50,  -11,  -41,   18,   -5,   35,  -38,  -52,
             -29,  -51,  -15,  -23,  -17,  -23,  -23,  -24,
             -37,  -27,  -39,  -31,  -22,  -14,   -2,  -30,
             -74,   -5,  -14,  -37,   -5,  -22,    3, -103,
             -35,  -30,  -10,    5,   16,    2,  -45,  -41,
        },
        { // queen
             -62,    5,   17,   67,   88,  105,   48,  -27,
               6,    4,    4,   47,   38,   41,    4,   21,
              -1,   14,   10,   16,   -7,   51,    6,   45,
             -14,  -21,  -14,  -37,  -62,  -37,  -39,  -11,
               6,  -10,   13,    3,  -23,    4,   -3,    9,
              22,   22,    5,   17,    9,    5,   34,   18,
              35,    8,   31,   47,   52,   48,   57,   18,
              20,  -46,   36,   36,   41,   10,   79,  -48,
        },
        { // king
             -16,   89,   22,    1,  -54,    5,   24,   16,
              38,    9,  -17,   17,   -9,   -1,  -13,  -27,
              -1,   24,    1,   -5,  -19, -104, -184,   -6,
             -38,  -19, -138,  -34,  -69,  -30,   -9,  -53,
             -83,  -73,  -68,  -41,  -79, -115,  -73, -114,
             -65,  -64, -170, -118,  -90, -100,  -28,  -37,
              12,  -27,  -22,  -59,  -47,  -24,   39,   27,
             -33,   33,   -4,  -50,   11,  -48,   30,    7,
        },
    };
    int pstEg[6][64] = {
        { // pawn
               0,    0,    0,    0,    0,    0,    0,    0,
             150,  148,  139,   86,  104,  133,  169,  182,
              83,   91,   46,   -1,  -30,   17,   46,   75,
              28,   39,   -3,  -31,  -31,    4,   17,   18,
              13,    9,  -21,   -7,  -16,  -21,   15,   -1,
              -2,   -2,  -18,  -15,  -19,   -7,   -9,  -17,
              13,    2,   -3,  -18,    1,  -10,  -18,  -10,
               0,    0,    0,    0,    0,    0,    0,    0,
        },
        { // knight
             -54,   24,  -55,  -20,  -29,  -12,  -93,  -48,
             -12,    7,  -22,   14,    8,  -29,    5,  -46,
              22,  -27,   12,    7,    7,  -20,   11,  -50,
              11,   44,   40,   30,   34,   17,   24,   -4,
               8,   10,   29,   46,   38,   28,    6,  -40,
             -12,   -7,   14,   13,    9,    7,  -17,   -3,
              -9,    5,   -8,    7,    4,   -8,  -25,  -40,
             -28,    5,  -15,  -22,    1,  -19,  -11,  -38,
        },
        { // bishop
              -9,  -26,  -15,    1,  -33,  -14,  -21,  -33,
              -7,  -32,   -3,  -26,   -2,  -13,   -9,  -81,
             -10,  -17,  -13,  -24,  -15,  -16,    0,   -8,
             -27,    8,   13,   14,    4,    9,   -4,  -32,
             -33,   14,   13,   10,    6,  -10,    6,  -13,
              -1,   -2,  -10,    5,   17,   -3,  -31,   -7,
             -17,  -24,  -17,  -25,  -20,  -22,  -35,  -45,
               1,  -17,  -13,  -28,  -25,   -6,   -9,  -15,
        },
        { // rook
              30,   13,   13,    3,   10,   33,    7,   23,
              10,   13,    3,   -2,   -8,   -2,    8,   -5,
              28,    6,   13,    4,  -10,   19,    0,   -8,
              31,   21,   35,   -4,    2,   -5,    7,   16,
              27,    8,   10,    0,   -3,   12,    0,  -16,
               2,   -3,   -4,   -1,   -6,  -10,  -29,   -5,
              17,   -7,    4,    0,  -36,  -21,  -19,  -12,
              -9,    2,   -4,  -11,  -25,  -31,   12,  -20,
        },
        { // queen
             108,  115,  141,  155,  132,   59,  101,  116,
             108,  169,  176,  124,  191,  148,  158,  140,
              72,  118,  186,  182,  199,  173,  143,   20,
             163,  207,  172,  248,  254,  206,  199,  119,
              25,  182,  160,  176,  196,  130,  115,   79,
              70,   63,  106,   97,  130,  165,   47,    7,
              65,   38,   35,   24,   -3,   19,  -37,  -26,
              52,   52,  -12,  -32,   -5,   48,  -12,   23,
        },
        { // king
             130,   -3,   37,   62,   -3,   63,   23,  -26,
              -4,   59,   80,   66,   70,   33,   46,   48,
              33,   48,   27,   47,   62,   14,   22,   31,
              31,   60,   66,   35,   39,   35,   43,   19,
              -9,   21,   39,   50,   42,   63,   30,    3,
              11,   27,   63,   46,   50,   47,   21,   -4,
             -27,    7,   14,   31,   23,   17,   -5,  -17,
             -55,  -45,  -21,  -10,  -40,  -14,  -34,  -55,
        },
    };

    // Mobility, indexed by the number of reachable squares that are neither
    // occupied by own pieces nor attacked by enemy pawns.
    int knightMobilityMg[9]  = { -34, -13, -6, -2, 5, 11, 18, 28, 25 };
    int knightMobilityEg[9]  = { -68, -20, 1, 12, 24, 32, 39, 33, 16 };
    int bishopMobilityMg[14] = { -47, -19, -5, 4, 11, 11, 11, 9, 19, 15, 26, 28, 18, 22 };
    int bishopMobilityEg[14] = { -96, -22, -13, -4, 8, 27, 32, 34, 34, 26, 33, 24, 29, 37 };
    int rookMobilityMg[15]   = { -51, -35, -27, -17, -12, 3, 7, 17, 9, 19, 34, 22, 20, 3, 12 };
    int rookMobilityEg[15]   = { -86, -41, -30, -13, -6, -4, 7, 10, 26, 26, 31, 32, 37, 32, 26 };
    // Queen that has left its back rank and can barely move.
    EvalPair queenNoMoves  = {-150, -100};  // 0 safe squares
    EvalPair queenFewMoves = {-80, -40};    // 1-2 safe squares

    // King safety: each attacked square in the enemy king zone adds the
    // attacker's weight to a danger count, which indexes kingSafetyTable.
    int kingAttackKnight = 2;
    int kingAttackBishop = 2;
    int kingAttackRook   = 3;
    int kingAttackQueen  = 4;
    int kingSafetyTable[100] = {
        -2, -2, -2, -2, 2, 8, 8, 8, 11, 11,
        11, 11, 24, 31, 42, 42, 43, 50, 50, 62,
        62, 80, 80, 89, 89, 105, 109, 131, 131, 131,
        167, 185, 185, 185, 188, 220, 220, 264, 264, 271,
        336, 336, 336, 339, 339, 339, 339, 339, 367, 404,
        404, 404, 412, 414, 424, 436, 448, 459, 471, 483,
        494, 500, 500, 500, 500, 500, 500, 500, 500, 500,
        500, 500, 500, 500, 500, 500, 500, 500, 500, 500,
        500, 500, 500, 500, 500, 500, 500, 500, 500, 500,
        500, 500, 500, 500, 500, 500, 500, 500, 500, 500,
    };

    // King shelter (middlegame only).
    int kingFileNoOwnPawn     = -26;  // king on a flank file with no own pawn on it
    int kingFileFullyOpen     = -77;  // ...and no enemy pawn either (extra)
    int kingAdjFilesNoOwnPawn = -29;  // no own pawn on the files next to the king
    int kingCenterOpenFile    = -83;  // king on a center file with no pawns at all
    int shieldPawnUnmoved     = 21;   // shield pawn on its starting rank
    int shieldPawnAdvanced    = 9;    // shield pawn one step forward
    int shieldPawnMissing     = -10;  // neither

    // Trapped pieces (halved deep in the endgame).
    int trappedBishop7th = -169;  // e.g. white bishop a7 with black pawn b6
    int trappedBishop6th = -17;   // e.g. white bishop a6 with black pawn b7
    int knightInCorner   = -25;   // knight on the enemy's a8/h8 corner
    int trappedKnight7th = -81;   // knight on a7 next to enemy pawns on b6/c6

    // A piece pinned to its own king by a rook, bishop or queen.
    EvalPair pinnedPiece = {-12, -44};

    // Threats: a pawn or minor piece attacking a more valuable enemy piece.
    EvalPair pawnThreatQueen  = {60, 42};
    EvalPair pawnThreatRook   = {123, 25};
    EvalPair pawnThreatMinor  = {77, 56};
    EvalPair minorThreatQueen = {60, 34};
    EvalPair minorThreatRook  = {75, 42};
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
