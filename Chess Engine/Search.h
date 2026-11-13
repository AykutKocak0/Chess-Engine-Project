#pragma once
#include <stdint.h>
#include "Types.h"
#include <string>
#include "Board.h"
#include <atomic>
#include "GenerateMoves.h"
#include <chrono>
#include <vector>
#include <thread>
#include "TimeManagement.h"

// Prints one line of UCI output. Safe to call from the search thread and the
// input thread at the same time (lines never interleave).
void uciPrint(const std::string& line);

// What a "go" command asked for.
struct SearchLimits {
    int depth = 64;
    int timeLeft = 0;    // our clock in ms (wtime/btime), 0 = not given
    int increment = 0;
    int movesToGo = 0;
    int moveTime = 0;    // fixed ms per move, 0 = not given
    bool infinite = false;
};

class Search {
public:
    ~Search() { stopAndWait(); }

    // Starts searching a copy of the board on a background thread and returns
    // immediately. A search that is still running is stopped first.
    void startAsync(const Board& board, const SearchLimits& limits);
    // Asks the running search to stop and waits until it has printed bestmove.
    void stopAndWait();
    // Waits for the running search to finish on its own.
    void waitForFinish();
    bool isInfinite() const { return infinite; }

    Move getBestMove() const { return bestMoveFound; }
    std::string moveToString(Move move);

private:
    void startSearch(Board& board, const SearchLimits& limits);

    std::atomic<bool> stopped{false};
    std::atomic<bool> infinite{false};
    std::thread worker;

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

    // Mate scores: being mated at ply p scores -MATE_SCORE + p. Anything beyond
    // +/-MATE_BOUND is a mate score (static evals never get near it).
    static constexpr int MATE_SCORE = 90000;
    static constexpr int MATE_BOUND = 80000;
    // UCI score string: "cp N", or "mate N" in moves (negative = getting mated).
    static std::string scoreToUci(int score);

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

    // Triangular PV table: pvTable[ply][ply .. pvLength[ply]-1] is the best line
    // found from the node at that ply. One extra row because a child at ply 64
    // still resets its length before returning. Kept last so this large, rarely
    // touched block does not sit between the hot move-ordering tables.
    int pvLength[65];
    Move pvTable[65][65];
};