#pragma once
#include <stdint.h>

namespace Constants {
    extern uint64_t FileMasks[8];
    extern uint64_t PassedPawnMasks[2][64];

    extern int reductionTable[64][64];

    extern uint64_t zobristPieces[12][64];
    extern uint64_t zobristSide;
    extern uint64_t zobristCastling[16];
    extern uint64_t zobristEnPassant[8];

    extern uint64_t kingNearSq[2][64];

    void initZobrist();
    void initEvaluationMasks();
    void initWhole();
    void initReductionTable();
    void initKingNearSq();
}


