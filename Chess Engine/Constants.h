#pragma once
#include <stdint.h>

namespace Constants {
    extern uint64_t FileMasks[8];
    extern uint64_t PassedPawnMasks[2][64];

    extern int mg_pawn_table[64];
    extern int eg_pawn_table[64];
    extern int mg_knight_table[64];
    extern int eg_knight_table[64];
    extern int mg_bishop_table[64];
    extern int eg_bishop_table[64];
    extern int mg_rook_table[64];
    extern int eg_rook_table[64];
    extern int mg_queen_table[64];
    extern int eg_queen_table[64];
    extern int mg_king_table[64];
    extern int eg_king_table[64];
    extern int* mgTables[6];
    extern int* egTables[6];

    extern int reductionTable[64][64];

    extern uint64_t zobristPieces[12][64];
    extern uint64_t zobristSide;
    extern uint64_t zobristCastling[16];
    extern uint64_t zobristEnPassant[8];

    extern uint64_t kingNearSq[2][64];

    void initZobrist();
    void initEvaluationMasks();
    void initPieceTables();
    void initWhole();
    void initReductionTable();
    void initKingNearSq();
}


