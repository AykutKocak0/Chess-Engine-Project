
#include "Constants.h"
#include <random>
#include <iostream>

namespace Constants {

    void initWhole(){
        initEvaluationMasks();
        initZobrist();
        initReductionTable();
        initKingNearSq();
    }
    
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

    uint64_t zobristPieces[12][64];
    uint64_t zobristSide;
    uint64_t zobristCastling[16];
    uint64_t zobristEnPassant[8];
    uint64_t PassedPawnMasks[2][64];
    int reductionTable[64][64];
    uint64_t kingNearSq[2][64];
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

    