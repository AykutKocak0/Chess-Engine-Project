#pragma once
#include <stdint.h>
#include "Types.h"
#include <string>
#include "Board.h"
#include <atomic>
#include "GenerateMoves.h"
#include <chrono>
#include <vector>
#include "TimeManagement.h"

class Search {
public:
    std::atomic<bool> stopped;
    void startSearch(Board& board, int maxDepth, int timeLeft, int increment, int movesToGo);
    void stop(){stopped = true;}
    Move getBestMove() const { return bestMoveFound; }
    std::string moveToString(Move move);

private:
    int alphaBeta(Board& board, int alpha, int beta, int depthLeft, int ply, bool allowNull);
    int quiescence(Board& board, int alpha, int beta, int ply, int qsDepth = 0);

    int scoreCapture(Board& board, Move& move, bool* losing = nullptr);
    int scoreMove(Board& board, Move& move, int depth);
    void updateHistory(const Board& board, const Move& move, int ply, int bonus);

    // Search stack: the move made at each ply and the piece that made it
    // (piece -1 = no usable move, e.g. a null move).
    Move stackMoves[64];
    int stackPieces[64];

    // Countermove: the quiet reply that last refuted [piece][to] of the previous move.
    Move counterMoves[12][64];

    // Continuation history: [previous piece][previous to][piece][to].
    // Heap-allocated (about 2.4 MB) because Search lives on the stack in main().
    std::vector<int> contHist = std::vector<int>(12 * 64 * 12 * 64, 0);
    int& contHistEntry(int prevPiece, int prevTo, int piece, int to) {
        return contHist[((prevPiece * 64 + prevTo) * 12 + piece) * 64 + to];
    }

    static constexpr int SCORE_COUNTER      =   700000;

    // Move-ordering score bands (see scoreMove).
    static constexpr int SCORE_HASH         =  2000000;
    static constexpr int SCORE_GOOD_CAPTURE =  1000000;
    static constexpr int SCORE_KILLER_1     =   900000;
    static constexpr int SCORE_KILLER_2     =   800000;
    static constexpr int SCORE_BAD_CAPTURE  = -1000000;
    static constexpr int SCORE_UNDERPROMO   = -2000000;
    static constexpr int HISTORY_MAX        =    10000;

    uint64_t betaCutoffs = 0;
    uint64_t firstMoveCutoffs = 0;
    void pickBestMove(Move* moveList, int count, int currentIndex);
    
    char getPromotionChar(Move move);

    GenerateMoves gen;

    TimeManagement timeManager;

    Move bestMoveFound;
    bool stopSearching = false;
    bool timed = false;
    int historyTable[2][64][64];

    Move killerMoves[64][2];

    const int MAX_PLY = 64;

    uint64_t nodes; 
    std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
};