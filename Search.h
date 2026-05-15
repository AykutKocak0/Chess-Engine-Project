#pragma once
#include <stdint.h>
#include "Types.h"
#include <string>
#include "Board.h"
#include <atomic>
#include "GenerateMoves.h"
#include <chrono>
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

    int scoreCapture(Board& board, Move& move);
    int scoreMove(Board& board, Move& move, int depth);
    void pickBestMove(Move* moveList, int count, int currentIndex);
    
    char getPromotionChar(Move move);

    GenerateMoves gen;

    TimeManagement timeManager;

    Move bestMoveFound;
    bool stopSearching = false;
    int historyTable[2][64][64];

    Move killerMoves[64][2];

    const int MAX_PLY = 64;

    uint64_t nodes; 
    std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
};