
#include "Constants.h"
#include <random>
#include <iostream>

namespace Constants {

    void initWhole(){
        initEvaluationMasks();
        initPieceTables();
        initZobrist();
        initReductionTable();
        initKingNearSq();
    }
    int* mgTables[6] = {nullptr};
    int* egTables[6] = {nullptr};
    
    uint64_t FileMasks[8] = {
        0x0101010101010101ULL,
        0x0202020202020202ULL,
        0x0404040404040404ULL, 
        0x0808080808080808ULL, 
        0x1010101010101010ULL,
        0x2020202020202020ULL, 
        0x4040404040404040ULL, 
        0x8080808080808080ULL  
    };
    
    int mg_pawn_table[64] = {
        0,   0,   0,   0,   0,   0,  0,   0,
        98, 134,  61,  95,  68, 126, 34, -11,
        -6,   7,  26,  31,  65,  56, 25, -20,
        -14,  13,   6,  21,  23,  12, 17, -23,
        -27,  -2,  -5,  12,  17,   6, 10, -25,
        -26,  -4,  -4, -10,   3,   3, 33, -12,
        -35,  -1, -20, -23, -15,  24, 38, -22,
        0,   0,   0,   0,   0,   0,  0,   0,
    };

    int eg_pawn_table[64] = {
        0,   0,   0,   0,   0,   0,   0,   0,
        178, 173, 158, 134, 147, 132, 165, 187,
        94, 100,  85,  67,  56,  53,  82,  84,
        32,  24,  13,   5,  -2,   4,  17,  17,
        13,   9,  -3,  -7,  -7,  -8,   3,  -1,
        4,   7,  -6,   1,   0,  -5,  -1,  -8,
        13,   8,   8,  10,  13,   0,   2,  -7,
        0,   0,   0,   0,   0,   0,   0,   0,
    };

    int mg_knight_table[64] = {
        -167, -89, -34, -49,  61, -97, -15, -107,
        -73, -41,  72,  36,  23,  62,   7,  -17,
        -47,  60,  37,  65,  84, 129,  73,   44,
        -9,  17,  19,  53,  37,  69,  18,   22,
        -13,   4,  16,  13,  28,  19,  21,   -8,
        -23,  -9,  12,  10,  19,  17,  25,  -16,
        -29, -53, -12,  -3,  -1,  18, -14,  -19,
        -105, -21, -58, -33, -17, -28, -19,  -23,
    };

    int eg_knight_table[64] = {
        -58, -38, -13, -28, -31, -27, -63, -99,
        -25,  -8, -25,  -2,  -9, -25, -24, -52,
        -24, -20,  10,   9,  -1,  -9, -19, -41,
        -17,   3,  22,  22,  22,  11,   8, -18,
        -18,  -6,  16,  25,  16,  17,   4, -18,
        -23,  -3,  -1,  15,  10,  -3, -20, -22,
        -42, -20, -10,  -5,  -2, -20, -23, -44,
        -29, -51, -23, -15, -22, -18, -50, -64,
    };

    int mg_bishop_table[64] = {
        -29,   4, -82, -37, -25, -42,   7,  -8,
        -26,  16, -18, -13,  30,  59,  18, -47,
        -16,  37,  43,  40,  35,  50,  37,  -2,
        -4,   5,  19,  50,  37,  37,   7,  -2,
        -6,  13,  13,  26,  34,  12,  10,   4,
        0,  15,  15,  15,  14,  27,  18,  10,
        4,  15,  16,   0,   7,  21,  33,   1,
        -33,  -3, -14, -21, -13, -12, -39, -21,
    };

    int eg_bishop_table[64] = {
        -14, -21, -11,  -8, -7,  -9, -17, -24,
        -8,  -4,   7, -12, -3, -13,  -4, -14,
        2,  -8,   0,  -1, -2,   6,   0,   4,
        -3,   9,  12,   9, 14,  10,   3,   2,
        -6,   3,  13,  19,  7,  10,  -3,  -9,
        -12,  -3,   8,  10, 13,   3,  -7, -15,
        -14, -18,  -7,  -1,  4,  -9, -15, -27,
        -23,  -9, -23,  -5, -9, -16,  -5, -17,
    };

    int mg_rook_table[64] = {
        32,  42,  32,  51, 63,  9,  31,  43,
        27,  32,  58,  62, 80, 67,  26,  44,
        -5,  19,  26,  36, 17, 45,  61,  16,
        -24, -11,   7,  26, 24, 35,  -8, -20,
        -36, -26, -12,  -1,  9, -7,   6, -23,
        -45, -25, -16, -17,  3,  0,  -5, -33,
        -44, -16, -20,  -9, -1, 11,  -6, -71,
        -19, -13,   1,  17, 16,  7, -37, -26,
    };

    int eg_rook_table[64] = {
        13, 10, 18, 15, 12,  12,   8,   5,
        11, 13, 13, 11, -3,   3,   8,   3,
        7,  7,  7,  5,  4,  -3,  -5,  -3,
        4,  3, 13,  1,  2,   1,  -1,   2,
        3,  5,  8,  4, -5,  -6,  -8, -11,
        -4,  0, -5, -1, -7, -12,  -8, -16,
        -6, -6,  0,  2, -9,  -9, -11,  -3,
        -9,  2,  3, -1, -5, -13,   4, -20,
    };

    int mg_queen_table[64] = {
        -28,   0,  29,  12,  59,  44,  43,  45,
        -24, -39,  -5,   1, -16,  57,  28,  54,
        -13, -17,   7,   8,  29,  56,  47,  57,
        -27, -27, -16, -16,  -1,  17,  -2,   1,
        -9, -26,  -9, -10,  -2,  -4,   3,  -3,
        -14,   2, -11,  -2,  -5,   2,  14,   5,
        -35,  -8,  11,   2,   8,  15,  -3,   1,
        -1, -18,  -9,  10, -15, -25, -31, -50,
    };

    int eg_queen_table[64] = {
        -9,  22,  22,  27,  27,  19,  10,  20,
        -17,  20,  32,  41,  58,  25,  30,   0,
        -20,   6,   9,  49,  47,  35,  19,   9,
        3,  22,  24,  45,  57,  40,  57,  36,
        -18,  28,  19,  47,  31,  34,  39,  23,
        -16, -27,  15,   6,   9,  17,  10,   5,
        -22, -23, -30, -16, -16, -23, -36, -32,
        -33, -28, -22, -43,  -5, -32, -20, -41,
    };

    int mg_king_table[64] = {
        -65,  23,  16, -15, -56, -34,   2,  13,
        29,  -1, -20,  -7,  -8,  -4, -38, -29,
        -9,  24,   2, -16, -20,   6,  22, -22,
        -17, -20, -12, -27, -30, -25, -14, -36,
        -49,  -1, -27, -39, -46, -44, -33, -51,
        -14, -14, -22, -46, -44, -30, -15, -27,
        1,   7,  -8, -64, -43, -16,   9,   8,
        -15,  36,  12, -54,   8, -28,  24,  14,
    };

    int eg_king_table[64] = {
        -74, -35, -18, -18, -11,  15,   4, -17,
        -12,  17,  14,  17,  17,  38,  23,  11,
        10,  17,  23,  15,  20,  45,  44,  13,
        -8,  22,  24,  27,  26,  33,  26,   3,
        -18,  -4,  21,  24,  27,  23,   9, -11,
        -19,  -3,  11,  21,  23,  16,   7,  -9,
        -27, -11,   4,  13,  14,   4,  -5, -17,
        -53, -34, -21, -11, -28, -14, -24, -43
    };
    
    uint64_t zobristPieces[12][64];
    uint64_t zobristSide;
    uint64_t zobristCastling[16];
    uint64_t zobristEnPassant[8];
    uint64_t PassedPawnMasks[2][64];
    int reductionTable[64][64];
    uint64_t kingNearSq[2][64];
    void initPieceTables() {
        int mgValues[6] = { 82, 337, 365, 477, 1025, 0 };
        int egValues[6] = { 94, 281, 297, 512, 936, 0 };

        mgTables[0] = mg_pawn_table;
        mgTables[1] = mg_knight_table;
        mgTables[2] = mg_bishop_table;
        mgTables[3] = mg_rook_table;
        mgTables[4] = mg_queen_table;
        mgTables[5] = mg_king_table;

        egTables[0] = eg_pawn_table;
        egTables[1] = eg_knight_table;
        egTables[2] = eg_bishop_table;
        egTables[3] = eg_rook_table;
        egTables[4] = eg_queen_table;
        egTables[5] = eg_king_table;

        for (int type = 0; type < 6; type++) {
            for (int sq = 0; sq < 64; sq++) {
                mgTables[type][sq] += mgValues[type];
                egTables[type][sq] += egValues[type];
            }
        }
    }

    void initReductionTable() {
        for (int depth = 1; depth < 64; depth++) {
            for (int moveCount = 1; moveCount < 64; moveCount++) {
                double r = 0.5 + log(depth) * log(moveCount) / 2.3;
                reductionTable[depth][moveCount] = (int)r;
            }
        }
    }
    
    void initEvaluationMasks() {
        for (int sq = 0; sq < 64; sq++) {
            int f = sq % 8;
            int r = sq / 8;

            uint64_t whiteMask = 0;
            uint64_t blackMask = 0;

            for (int targetSq = 0; targetSq < 64; targetSq++) {
                int tf = targetSq % 8;
                int tr = targetSq / 8;
                if (tf >= f - 1 && tf <= f + 1) {
                    if (tr > r) whiteMask |= (1ULL << targetSq);
                    
                    if (tr < r) blackMask |= (1ULL << targetSq);
                }
            }
            PassedPawnMasks[0][sq] = whiteMask;
            PassedPawnMasks[1][sq] = blackMask;
        }
    }
    
    void initKingNearSq() {
        for (int sq = 0; sq < 64; sq++) {
            uint64_t b = 1ULL << sq;
            
            uint64_t ring = 0;
            uint64_t whiteExtra = 0;
            uint64_t blackExtra = 0;

            ring |= (b << 8);  
            ring |= (b >> 8);  
            
            if (b & ~FileMasks[0]) { 
                ring |= (b << 7);  
                ring |= (b >> 1);  
                ring |= (b >> 9);  
                
                
                whiteExtra |= (b << 15); 
                whiteExtra |= (b << 23); 
                
                
                blackExtra |= (b >> 17); 
                blackExtra |= (b >> 25); 
            }
            
        
            if (b & ~FileMasks[7]) { 
                ring |= (b << 9);  
                ring |= (b << 1);  
                ring |= (b >> 7);  
                
                
                whiteExtra |= (b << 17); 
                whiteExtra |= (b << 25); 
                
               
                blackExtra |= (b >> 15); 
                blackExtra |= (b >> 23); 
            }

            whiteExtra |= (b << 16); 
            whiteExtra |= (b << 24); 
            blackExtra |= (b >> 16); 
            blackExtra |= (b >> 24); 
            
            kingNearSq[0][sq] = ring | whiteExtra; 
            kingNearSq[1][sq] = ring | blackExtra; 
        }
    }

    void initZobrist(){
        std::mt19937_64 gen(2230765027ULL); 
        std::uniform_int_distribution<uint64_t> dist;

        for (int p = 0; p < 12; p++) {
            for (int s = 0; s < 64; s++) {
                zobristPieces[p][s] = dist(gen);
                
            }
        }

        zobristSide = dist(gen);

        for (int i = 0; i < 16; i++) zobristCastling[i] = dist(gen);
        for (int i = 0; i < 8; i++) zobristEnPassant[i] = dist(gen);
    }
}

    