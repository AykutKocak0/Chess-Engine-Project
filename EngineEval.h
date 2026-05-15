#pragma once
#include <stdint.h>
#include "Types.h"
#include <string>
#include "Board.h"

class EngineEval {
public:
    static int evaluate(const Board& board);
    static bool isEndGame(const Board& board);
    static int getPhase(const Board& board);

private:
    static int evaluateMaterial(const Board& board, int phase);
    static int taperedEval(const Board& board);
    static int bishopPair(const Board& board, int phase);

    static int evaluatePawnStructure(const Board& board, int phase, uint64_t whitePawnAttacks, uint64_t blackPawnAttacks);
    static int evaluatePieceSquareTable(const Board& board, int phase);
    static int mopUpEval(const Board& board, Color winner, int phase, bool bishopKnightEndgame);
    static int pureEvaluateMaterial(const Board& board, int phase);
    static int kingPawnStructure(const Board& board, Color color, int phase);
    static int mobilityEval(const Board& board, int phase, Color color, uint64_t enemyPawns);
    static int checkTrappedPatterns(const Board& board, int phase, Color color);
    static int checkPinnedPieces(const Board& board, int phase);
    static int otherPawn(const Board& board, int phase, uint64_t w, uint64_t s);
    static int kingSafetyEval(const Board& board, int phase);
    static int underDevelopmentPenalty(const Board& board, int phase);
    static int threatEval(const Board& board, int phase, uint64_t whitePawnAttacks, uint64_t blackPawnAttacks);



    static uint64_t enemyPawnAttacks(const Board& board, Color enemyColor);
    static constexpr uint64_t WHITE_SQUARES = 0x55AA55AA55AA55AAULL;
    static constexpr uint64_t BLACK_SQUARES = 0xAA55AA55AA55AA55ULL; 

    
    inline static bool isWhiteSquare(int sq) {
        return (1ULL << sq) & WHITE_SQUARES;
    }
};
