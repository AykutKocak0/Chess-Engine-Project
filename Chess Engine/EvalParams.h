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

struct EvalPair {
    int mg;
    int eg;
};

struct EvalParams {
    // Bonus for the side to move.
    int tempo = 20;

    // Material, indexed pawn, knight, bishop, rook, queen.
    EvalPair material[5] = { {100, 125}, {320, 285}, {330, 350}, {500, 600}, {900, 1000} };
    EvalPair bishopPair = {20, 50};

    // Pawn structure.
    EvalPair centerPawn          = {12, 5};     // pawn on d4/e4 (d5/e5 for Black)
    EvalPair centerPawnDuo       = {12, 6};     // both center pawns
    EvalPair doubledPawn         = {-10, -20};  // per file holding 2+ own pawns
    EvalPair isolatedPawn        = {-10, -20};  // per isolated pawn
    EvalPair protectedPassedPawn = {10, 25};    // passed pawn defended by a pawn
    // Passed pawn bonus by relative rank (0 = own back rank). A candidate passer
    // gets half of the bonus for the rank in front of it.
    int passedPawnMg[8] = { 0, 5, 10, 20, 40, 70, 100, 0 };
    int passedPawnEg[8] = { 0, 10, 25, 50, 100, 160, 260, 0 };

    // Piece-square tables [piece][square], piece order pawn..king. Square 0 is a8
    // (ChessProgramming wiki layout): White looks up sq ^ 56, Black looks up sq.
    int pstMg[6][64] = {
        { // pawn
               0,    0,    0,    0,    0,    0,    0,    0,
              98,  134,   61,   95,   68,  126,   34,  -11,
              -6,    7,   26,   31,   65,   56,   25,  -20,
             -14,   13,    6,   21,   23,   12,   17,  -23,
             -27,   -2,   -5,   12,   17,    6,   10,  -25,
             -26,   -4,   -4,  -10,    3,    3,   33,  -12,
             -35,   -1,  -20,  -23,  -15,   24,   38,  -22,
               0,    0,    0,    0,    0,    0,    0,    0,
        },
        { // knight
            -167,  -89,  -34,  -49,   61,  -97,  -15, -107,
             -73,  -41,   72,   36,   23,   62,    7,  -17,
             -47,   60,   37,   65,   84,  129,   73,   44,
              -9,   17,   19,   53,   37,   69,   18,   22,
             -13,    4,   16,   13,   28,   19,   21,   -8,
             -23,   -9,   12,   10,   19,   17,   25,  -16,
             -29,  -53,  -12,   -3,   -1,   18,  -14,  -19,
            -105,  -21,  -58,  -33,  -17,  -28,  -19,  -23,
        },
        { // bishop
             -29,    4,  -82,  -37,  -25,  -42,    7,   -8,
             -26,   16,  -18,  -13,   30,   59,   18,  -47,
             -16,   37,   43,   40,   35,   50,   37,   -2,
              -4,    5,   19,   50,   37,   37,    7,   -2,
              -6,   13,   13,   26,   34,   12,   10,    4,
               0,   15,   15,   15,   14,   27,   18,   10,
               4,   15,   16,    0,    7,   21,   33,    1,
             -33,   -3,  -14,  -21,  -13,  -12,  -39,  -21,
        },
        { // rook
              32,   42,   32,   51,   63,    9,   31,   43,
              27,   32,   58,   62,   80,   67,   26,   44,
              -5,   19,   26,   36,   17,   45,   61,   16,
             -24,  -11,    7,   26,   24,   35,   -8,  -20,
             -36,  -26,  -12,   -1,    9,   -7,    6,  -23,
             -45,  -25,  -16,  -17,    3,    0,   -5,  -33,
             -44,  -16,  -20,   -9,   -1,   11,   -6,  -71,
             -19,  -13,    1,   17,   16,    7,  -37,  -26,
        },
        { // queen
             -28,    0,   29,   12,   59,   44,   43,   45,
             -24,  -39,   -5,    1,  -16,   57,   28,   54,
             -13,  -17,    7,    8,   29,   56,   47,   57,
             -27,  -27,  -16,  -16,   -1,   17,   -2,    1,
              -9,  -26,   -9,  -10,   -2,   -4,    3,   -3,
             -14,    2,  -11,   -2,   -5,    2,   14,    5,
             -35,   -8,   11,    2,    8,   15,   -3,    1,
              -1,  -18,   -9,   10,  -15,  -25,  -31,  -50,
        },
        { // king
             -65,   23,   16,  -15,  -56,  -34,    2,   13,
              29,   -1,  -20,   -7,   -8,   -4,  -38,  -29,
              -9,   24,    2,  -16,  -20,    6,   22,  -22,
             -17,  -20,  -12,  -27,  -30,  -25,  -14,  -36,
             -49,   -1,  -27,  -39,  -46,  -44,  -33,  -51,
             -14,  -14,  -22,  -46,  -44,  -30,  -15,  -27,
               1,    7,   -8,  -64,  -43,  -16,    9,    8,
             -15,   36,   12,  -54,    8,  -28,   24,   14,
        },
    };
    int pstEg[6][64] = {
        { // pawn
               0,    0,    0,    0,    0,    0,    0,    0,
             178,  173,  158,  134,  147,  132,  165,  187,
              94,  100,   85,   67,   56,   53,   82,   84,
              32,   24,   13,    5,   -2,    4,   17,   17,
              13,    9,   -3,   -7,   -7,   -8,    3,   -1,
               4,    7,   -6,    1,    0,   -5,   -1,   -8,
              13,    8,    8,   10,   13,    0,    2,   -7,
               0,    0,    0,    0,    0,    0,    0,    0,
        },
        { // knight
             -58,  -38,  -13,  -28,  -31,  -27,  -63,  -99,
             -25,   -8,  -25,   -2,   -9,  -25,  -24,  -52,
             -24,  -20,   10,    9,   -1,   -9,  -19,  -41,
             -17,    3,   22,   22,   22,   11,    8,  -18,
             -18,   -6,   16,   25,   16,   17,    4,  -18,
             -23,   -3,   -1,   15,   10,   -3,  -20,  -22,
             -42,  -20,  -10,   -5,   -2,  -20,  -23,  -44,
             -29,  -51,  -23,  -15,  -22,  -18,  -50,  -64,
        },
        { // bishop
             -14,  -21,  -11,   -8,   -7,   -9,  -17,  -24,
              -8,   -4,    7,  -12,   -3,  -13,   -4,  -14,
               2,   -8,    0,   -1,   -2,    6,    0,    4,
              -3,    9,   12,    9,   14,   10,    3,    2,
              -6,    3,   13,   19,    7,   10,   -3,   -9,
             -12,   -3,    8,   10,   13,    3,   -7,  -15,
             -14,  -18,   -7,   -1,    4,   -9,  -15,  -27,
             -23,   -9,  -23,   -5,   -9,  -16,   -5,  -17,
        },
        { // rook
              13,   10,   18,   15,   12,   12,    8,    5,
              11,   13,   13,   11,   -3,    3,    8,    3,
               7,    7,    7,    5,    4,   -3,   -5,   -3,
               4,    3,   13,    1,    2,    1,   -1,    2,
               3,    5,    8,    4,   -5,   -6,   -8,  -11,
              -4,    0,   -5,   -1,   -7,  -12,   -8,  -16,
              -6,   -6,    0,    2,   -9,   -9,  -11,   -3,
              -9,    2,    3,   -1,   -5,  -13,    4,  -20,
        },
        { // queen
              -9,   22,   22,   27,   27,   19,   10,   20,
             -17,   20,   32,   41,   58,   25,   30,    0,
             -20,    6,    9,   49,   47,   35,   19,    9,
               3,   22,   24,   45,   57,   40,   57,   36,
             -18,   28,   19,   47,   31,   34,   39,   23,
             -16,  -27,   15,    6,    9,   17,   10,    5,
             -22,  -23,  -30,  -16,  -16,  -23,  -36,  -32,
             -33,  -28,  -22,  -43,   -5,  -32,  -20,  -41,
        },
        { // king
             -74,  -35,  -18,  -18,  -11,   15,    4,  -17,
             -12,   17,   14,   17,   17,   38,   23,   11,
              10,   17,   23,   15,   20,   45,   44,   13,
              -8,   22,   24,   27,   26,   33,   26,    3,
             -18,   -4,   21,   24,   27,   23,    9,  -11,
             -19,   -3,   11,   21,   23,   16,    7,   -9,
             -27,  -11,    4,   13,   14,    4,   -5,  -17,
             -53,  -34,  -21,  -11,  -28,  -14,  -24,  -43,
        },
    };

    // Mobility, indexed by the number of reachable squares that are neither
    // occupied by own pieces nor attacked by enemy pawns.
    int knightMobilityMg[9]  = { -20, -15, -8, -2, 2, 8, 12, 16, 20 };
    int knightMobilityEg[9]  = { -40, -30, -15, -5, 5, 15, 25, 35, 40 };
    int bishopMobilityMg[14] = { -20, -12, -8, -2, 2, 8, 12, 18, 20, 22, 25, 25, 25, 25 };
    int bishopMobilityEg[14] = { -40, -25, -15, -5, 5, 15, 25, 35, 40, 45, 50, 50, 50, 50 };
    int rookMobilityMg[15]   = { -20, -12, -8, -5, -2, 0, 2, 5, 8, 12, 15, 18, 20, 20, 20 };
    int rookMobilityEg[15]   = { -40, -25, -15, -10, -5, 0, 5, 10, 15, 20, 25, 30, 35, 40, 40 };
    // Queen that has left its back rank and can barely move.
    EvalPair queenNoMoves  = {-150, -100};  // 0 safe squares
    EvalPair queenFewMoves = {-80, -40};    // 1-2 safe squares

    // King safety: each attacked square in the enemy king zone adds the
    // attacker's weight to a danger count, which indexes kingSafetyTable.
    int kingAttackKnight = 2;
    int kingAttackBishop = 2;
    int kingAttackRook   = 3;
    int kingAttackQueen  = 5;
    int kingSafetyTable[100] = {
           0,    0,    1,    2,    3,    5,    7,    9,   12,   15,
          18,   22,   26,   30,   35,   39,   44,   50,   56,   62,
          68,   75,   82,   85,   89,   97,  105,  113,  122,  131,
         140,  150,  169,  180,  191,  202,  213,  225,  237,  248,
         260,  272,  283,  295,  307,  319,  330,  342,  354,  366,
         377,  389,  401,  412,  424,  436,  448,  459,  471,  483,
         494,  500,  500,  500,  500,  500,  500,  500,  500,  500,
         500,  500,  500,  500,  500,  500,  500,  500,  500,  500,
         500,  500,  500,  500,  500,  500,  500,  500,  500,  500,
         500,  500,  500,  500,  500,  500,  500,  500,  500,  500,
    };

    // King shelter (middlegame only).
    int kingFileNoOwnPawn     = -15;  // king on a flank file with no own pawn on it
    int kingFileFullyOpen     = -10;  // ...and no enemy pawn either (extra)
    int kingAdjFilesNoOwnPawn = -10;  // no own pawn on the files next to the king
    int kingCenterOpenFile    = -30;  // king on a center file with no pawns at all
    int shieldPawnUnmoved     = 20;   // shield pawn on its starting rank
    int shieldPawnAdvanced    = 10;   // shield pawn one step forward
    int shieldPawnMissing     = -10;  // neither

    // Trapped pieces (halved deep in the endgame).
    int trappedBishop7th = -80;  // e.g. white bishop a7 with black pawn b6
    int trappedBishop6th = -50;  // e.g. white bishop a6 with black pawn b7
    int knightInCorner   = -40;  // knight on the enemy's a8/h8 corner
    int trappedKnight7th = -30;  // knight on a7 next to enemy pawns on b6/c6

    // A piece pinned to its own king by a rook, bishop or queen.
    EvalPair pinnedPiece = {-20, -42};

    // Threats: a pawn or minor piece attacking a more valuable enemy piece.
    EvalPair pawnThreatQueen  = {50, 40};
    EvalPair pawnThreatRook   = {30, 25};
    EvalPair pawnThreatMinor  = {15, 15};
    EvalPair minorThreatQueen = {35, 25};
    EvalPair minorThreatRook  = {15, 10};
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
