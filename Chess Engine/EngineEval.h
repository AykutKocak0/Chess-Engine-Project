#pragma once
#include <stdint.h>
#include "Types.h"
#include <string>
#include "Board.h"

// Squares attacked by one side's pieces (pawns and king not included), split by
// piece type for the safe-check test, and how many pieces hit the enemy king zone.
struct AttackInfo {
    uint64_t all = 0;
    uint64_t knights = 0;
    uint64_t bishops = 0;
    uint64_t rooks = 0;
    uint64_t queens = 0;
    int kingZoneAttackers = 0;
};

class EngineEval {
public:
    static int evaluate(const Board& board);
    static bool isEndGame(const Board& board);
    static int getPhase(const Board& board);
    // The tuner turns this off: it evaluates from several threads at once and
    // changes the weights, so cached pawn scores would be shared and stale.
    static inline bool usePawnHash = true;

private:
    static int evaluateMaterial(const Board& board, int phase);
    static int taperedEval(const Board& board);
    static int bishopPair(const Board& board, int phase);

    static int evaluatePawnStructure(const Board& board, int phase, uint64_t whitePawnAttacks, uint64_t blackPawnAttacks);
    static int evaluatePieceSquareTable(const Board& board, int phase);
    static int mopUpEval(const Board& board, Color winner, int phase, bool bishopKnightEndgame);
    static int pureEvaluateMaterial(const Board& board, int phase);
    static int kingPawnStructure(const Board& board, Color color, int phase);
    static uint64_t evaluatePieceActivity(const Board& board, Color color, uint64_t enemyPawnAttacks, uint64_t enemyKingZone, int& mgMobility, int& egMobility, int& kingDanger, AttackInfo& info);
    static int safeCheckDanger(const Board& board, Color defender, const AttackInfo& attacker, uint64_t defenderAttacks);
    static int passedPawnDynamic(const Board& board, int phase, uint64_t whiteAttacks, uint64_t blackAttacks);
    static int scaleEndgame(const Board& board, int phase, int score);
    static uint64_t kingAttacks(int sq);
    static int checkTrappedPatterns(const Board& board, int phase, Color color);
    static int checkPinnedPieces(const Board& board, int phase);
    static int otherPawn(const Board& board, int phase, uint64_t w, uint64_t s);
    static int kingSafetyEval(int phase, int dangerToBlack, int dangerToWhite);
    static int underDevelopmentPenalty(const Board& board, int phase);
    static int threatEval(const Board& board, int phase, uint64_t whitePawnAttacks, uint64_t blackPawnAttacks, uint64_t whiteMinorAttacks, uint64_t blackMinorAttacks);
    static int outpostEval(const Board& board, int phase, uint64_t whitePawnAttacks, uint64_t blackPawnAttacks);



    static uint64_t enemyPawnAttacks(const Board& board, Color enemyColor);
    static constexpr uint64_t WHITE_SQUARES = 0x55AA55AA55AA55AAULL;
    static constexpr uint64_t BLACK_SQUARES = 0xAA55AA55AA55AA55ULL; 

    
    inline static bool isWhiteSquare(int sq) {
        return (1ULL << sq) & WHITE_SQUARES;
    }
};
