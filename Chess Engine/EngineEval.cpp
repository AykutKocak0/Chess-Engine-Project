#include <stdint.h>
#include "Types.h"
#include <string>
#include "Board.h"
#include "EngineEval.h"
#include "Constants.h"
#include <iostream>
#include "GenerateMoves.h"
#include "TransPositionTable.h"
using namespace Constants;

const int mgPassedBonus[8] = { 0, 5, 10, 20, 40, 70, 100, 0 };
const int egPassedBonus[8] = { 0, 10, 30, 60, 120, 200, 350, 0 };
const int knightEgMobility[9]  = { -40, -30, -15,  -5,   5,  15,  25,  35,  40 };
const int knightMgMobility[9]  = { -20, -15,  -8,  -2,   2,   8,  12,  16,  20 };

const int bishopEgMobility[14] = { -40, -25, -15,  -5,   5,  15,  25,  35,  40,  45,  50,  50,  50,  50 };
const int bishopMgMobility[14] = { -20, -12,  -8,  -2,   2,   8,  12,  18,  20,  22,  25,  25,  25,  25 };

const int rookEgMobility[15]   = { -40, -25, -15, -10,  -5,   0,   5,  10,  15,  20,  25,  30,  35,  40,  40 };
const int rookMgMobility[15]   = { -20, -12,  -8,  -5,  -2,   0,   2,   5,   8,  12,  15,  18,  20,  20,  20 };
uint64_t FILE_H = 0x8080808080808080ULL;
uint64_t FILE_A = 0x0101010101010101ULL;

static const int SafetyTable[100] = {
    0,  0,   1,   2,   3,   5,   7,   9,  12,  15,
  18,  22,  26,  30,  35,  39,  44,  50,  56,  62,
  68,  75,  82,  85,  89,  97, 105, 113, 122, 131,
 140, 150, 169, 180, 191, 202, 213, 225, 237, 248,
 260, 272, 283, 295, 307, 319, 330, 342, 354, 366,
 377, 389, 401, 412, 424, 436, 448, 459, 471, 483,
 494, 500, 500, 500, 500, 500, 500, 500, 500, 500,
 500, 500, 500, 500, 500, 500, 500, 500, 500, 500,
 500, 500, 500, 500, 500, 500, 500, 500, 500, 500,
 500, 500, 500, 500, 500, 500, 500, 500, 500, 500
};


int EngineEval::evaluate(const Board& board){
    int score = 0;
    if(board.sideToMove == WHITE) score+=20;
    else score-=20;
    uint64_t whitePawnAttacks = enemyPawnAttacks(board, WHITE);
    uint64_t blackPawnAttacks = enemyPawnAttacks(board, BLACK);
    bool bishopKnightEndgame = false;


    int whitePieces = (int) __builtin_popcountll(board.getOccupancy(WHITE));
    int blackPieces = (int) __builtin_popcountll(board.getOccupancy(BLACK));

    
    if (whitePieces + blackPieces == 2) return 0;


    if (whitePieces + blackPieces == 3) {
        if (board.getPieceBB(W_KNIGHT) || board.getPieceBB(B_KNIGHT) ||
            board.getPieceBB(W_BISHOP) || board.getPieceBB(B_BISHOP)) {
            return 0;
        }
    }
    if(whitePieces + blackPieces == 4){
        int wKnight = (int) __builtin_popcountll(board.getPieceBB(W_KNIGHT));
        int wBishop = (int) __builtin_popcountll(board.getPieceBB(W_BISHOP));
        int bKnight = (int) __builtin_popcountll(board.getPieceBB(B_KNIGHT));
        int bBishop = (int) __builtin_popcountll(board.getPieceBB(B_BISHOP));
        if(wKnight == 1 && wBishop == 1 && blackPieces == 1) bishopKnightEndgame = true;
        else if(bKnight == 1 && bBishop == 1 && whitePieces == 1) bishopKnightEndgame = true;
    }
    int phase = taperedEval(board);
    
    int materialDiff=pureEvaluateMaterial(board,phase);
    
    score+= evaluateMaterial(board, phase);
    
    score+=evaluatePawnStructure(board,phase,whitePawnAttacks,blackPawnAttacks);
    
    score+=bishopPair(board,phase);

    score+=evaluatePieceSquareTable(board,phase);

    // Mop up
    if(phase >150){
        Color winner = (materialDiff > 200) ? WHITE : BLACK;
        int mopUp=mopUpEval(board, winner, phase, bishopKnightEndgame);
        score += (mopUp * (phase - 150)) / (256 - 150);
    }
    
    score+=kingPawnStructure(board,WHITE,phase) - kingPawnStructure(board,BLACK,phase);

    score+= checkTrappedPatterns(board, phase, WHITE) - checkTrappedPatterns(board, phase, BLACK);

    score+= mobilityEval(board, phase, WHITE, blackPawnAttacks) - mobilityEval(board, phase, BLACK, whitePawnAttacks);

    score+= checkPinnedPieces(board,phase);
    
    score+= kingSafetyEval(board,phase);
    
    score+=underDevelopmentPenalty(board,phase);
    
    score+=threatEval(board,phase,whitePawnAttacks,blackPawnAttacks);
    
    return (board.sideToMove == WHITE) ? score : -score;
}

int EngineEval::taperedEval(const Board& board) {

    int TotalPhase = PawnPhase * 16 + KnightPhase * 4 + BishopPhase * 4 + RookPhase * 4 + QueenPhase * 2;
    int phase = TotalPhase;

    phase -= (int)__builtin_popcountll(board.getPieceBB(W_PAWN))   * PawnPhase;
    phase -= (int)__builtin_popcountll(board.getPieceBB(W_KNIGHT)) * KnightPhase;
    phase -= (int)__builtin_popcountll(board.getPieceBB(W_BISHOP)) * BishopPhase;
    phase -= (int)__builtin_popcountll(board.getPieceBB(W_ROOK))   * RookPhase;
    phase -= (int)__builtin_popcountll(board.getPieceBB(W_QUEEN))  * QueenPhase;
    phase -= (int)__builtin_popcountll(board.getPieceBB(B_PAWN))   * PawnPhase;
    phase -= (int)__builtin_popcountll(board.getPieceBB(B_KNIGHT)) * KnightPhase;
    phase -= (int)__builtin_popcountll(board.getPieceBB(B_BISHOP)) * BishopPhase;
    phase -= (int)__builtin_popcountll(board.getPieceBB(B_ROOK))   * RookPhase;
    phase -= (int)__builtin_popcountll(board.getPieceBB(B_QUEEN))  * QueenPhase;
    if (phase < 0) phase = 0;
    if (TotalPhase == 0) return 0; 

    return (phase * 256 + (TotalPhase / 2)) / TotalPhase;
}

int EngineEval::evaluateMaterial(const Board& board, int phase) {
    int mgScore = 0;
    int egScore = 0;

    int wP = (int)__builtin_popcountll(board.getPieceBB(W_PAWN));
    int wN = (int)__builtin_popcountll(board.getPieceBB(W_KNIGHT));
    int wB = (int)__builtin_popcountll(board.getPieceBB(W_BISHOP));
    int wR = (int)__builtin_popcountll(board.getPieceBB(W_ROOK));
    int wQ = (int)__builtin_popcountll(board.getPieceBB(W_QUEEN));

    int bP = (int)__builtin_popcountll(board.getPieceBB(B_PAWN));
    int bN = (int)__builtin_popcountll(board.getPieceBB(B_KNIGHT));
    int bB = (int)__builtin_popcountll(board.getPieceBB(B_BISHOP));
    int bR = (int)__builtin_popcountll(board.getPieceBB(B_ROOK));
    int bQ = (int)__builtin_popcountll(board.getPieceBB(B_QUEEN));

    mgScore += (wP - bP) * VAL_PAWN_MG;
    mgScore += (wN - bN) * VAL_KNIGHT_MG;
    mgScore += (wB - bB) * VAL_BISHOP_MG;
    mgScore += (wR - bR) * VAL_ROOK_MG;
    mgScore += (wQ - bQ) * VAL_QUEEN_MG;

    egScore += (wP - bP) * VAL_PAWN_EG;
    egScore += (wN - bN) * VAL_KNIGHT_EG;
    egScore += (wB - bB) * VAL_BISHOP_EG;
    egScore += (wR - bR) * VAL_ROOK_EG; 
    egScore += (wQ - bQ) * VAL_QUEEN_EG;
    return (((256-phase)*mgScore) + (egScore*phase)) / 256;
}

int EngineEval::pureEvaluateMaterial(const Board& board, int phase) {
    int mgScore = 0;

    int wP = (int)__builtin_popcountll(board.getPieceBB(W_PAWN));
    int wN = (int)__builtin_popcountll(board.getPieceBB(W_KNIGHT));
    int wB = (int)__builtin_popcountll(board.getPieceBB(W_BISHOP));
    int wR = (int)__builtin_popcountll(board.getPieceBB(W_ROOK));
    int wQ = (int)__builtin_popcountll(board.getPieceBB(W_QUEEN));

    int bP = (int)__builtin_popcountll(board.getPieceBB(B_PAWN));
    int bN = (int)__builtin_popcountll(board.getPieceBB(B_KNIGHT));
    int bB = (int)__builtin_popcountll(board.getPieceBB(B_BISHOP));
    int bR = (int)__builtin_popcountll(board.getPieceBB(B_ROOK));
    int bQ = (int)__builtin_popcountll(board.getPieceBB(B_QUEEN));

    mgScore += (wP - bP) * VAL_PAWN_MG;
    mgScore += (wN - bN) * VAL_KNIGHT_MG;
    mgScore += (wB - bB) * VAL_BISHOP_MG;
    mgScore += (wR - bR) * VAL_ROOK_MG;
    mgScore += (wQ - bQ) * VAL_QUEEN_MG;
    return mgScore;
}

int EngineEval::bishopPair(const Board& board, int phase){
    int mgBonus = 0;
    int egBonus = 0;

    if (__builtin_popcountll(board.getPieceBB(W_BISHOP)) >= 2) {
        mgBonus += 20;
        egBonus += 50;
    }
    if (__builtin_popcountll(board.getPieceBB(B_BISHOP)) >= 2) {
        mgBonus -= 20;
        egBonus -= 50;
    }

    int finalBonus = ((mgBonus * (256 - phase)) + (egBonus * phase)) / 256;
    
    return finalBonus;
}

int EngineEval::evaluatePawnStructure(const Board& board, int phase, uint64_t whitePawnAttacks, uint64_t blackPawnAttacks) {
    int mgScore = 0, egScore = 0;
    uint64_t pawnKey = board.getPawnKey();
    PawnEntry* entry = PTT.pawnProbe(pawnKey);
    
    if (entry != nullptr && entry->key == pawnKey) {
        mgScore = entry->mgScore;
        egScore = entry->egScore;
        
    }
    else {
        uint64_t whitePawns = board.getPieceBB(W_PAWN);
        uint64_t blackPawns = board.getPieceBB(B_PAWN);
        

        if (whitePawns & (1ULL << 28)) { mgScore += 25; egScore += 10; } // e4
        if (whitePawns & (1ULL << 27)) { mgScore += 25; egScore += 10; } // d4
        if (blackPawns & (1ULL << 36)) { mgScore -= 25; egScore -= 10; } // e5
        if (blackPawns & (1ULL << 35)) { mgScore -= 25; egScore -= 10; } // d5
        
        if ((whitePawns & (1ULL << 28)) && (whitePawns & (1ULL << 27))) { mgScore += 18; egScore += 9; }
        if ((blackPawns & (1ULL << 36)) && (blackPawns & (1ULL << 35))) { mgScore -= 18; egScore -= 9; }

        for (int i = 0; i < 8; i++) {
            uint64_t wPawnsOnFile = whitePawns & FileMasks[i];
            uint64_t bPawnsOnFile = blackPawns & FileMasks[i];

            if (wPawnsOnFile & (wPawnsOnFile - 1)) { mgScore -= 20; egScore -= 25; }
            if (bPawnsOnFile & (bPawnsOnFile - 1)) { mgScore += 20; egScore += 25; }

            uint64_t adj = 0;
            if (i > 0) adj |= FileMasks[i - 1];
            if (i < 7) adj |= FileMasks[i + 1];

            uint64_t tempW = wPawnsOnFile;
            while (tempW) {
                int whiteSq = getLS1B(tempW);

                if (!(whitePawns & adj)) { 
                    mgScore -= 15; 
                    egScore -= 25; 
                }

                if (!(blackPawns & PassedPawnMasks[WHITE][whiteSq])) {
                    int rank = whiteSq / 8;
                    mgScore += mgPassedBonus[rank];
                    egScore += egPassedBonus[rank];
                    
                    if (whitePawnAttacks & (1ULL << whiteSq)) { 
                        mgScore += 25; 
                        egScore += 60; 
                    }
                }
                else if(whiteSq < 56 && !(blackPawns & PassedPawnMasks[WHITE][whiteSq + 8]) && !(blackPawns & (1ULL << (whiteSq + 8)))) {
                    int rank = (whiteSq + 8) / 8;
                    mgScore += mgPassedBonus[rank] / 2; 
                    egScore += egPassedBonus[rank] / 2; 
                }
                popBit(tempW, whiteSq); 
            }

            uint64_t tempB = bPawnsOnFile;
            while (tempB) {
                int blackSq = getLS1B(tempB);

                if (!(blackPawns & adj)) { 
                    mgScore += 15; 
                    egScore += 25; 
                } 
                
                if (!(whitePawns & PassedPawnMasks[BLACK][blackSq])) {
                    int rank = blackSq / 8; 
                    mgScore -= mgPassedBonus[7 - rank];
                    egScore -= egPassedBonus[7 - rank];

                    if (blackPawnAttacks & (1ULL << blackSq)) { 
                        mgScore -= 25; 
                        egScore -= 60; 
                    }
                }
                else if(blackSq > 7 && !(whitePawns & PassedPawnMasks[BLACK][blackSq - 8]) && !(whitePawns & (1ULL << (blackSq - 8)))) {
                    int rank = (blackSq - 8) / 8;
                    
                    mgScore -= mgPassedBonus[7 - rank] / 2;
                    egScore -= egPassedBonus[7 - rank] / 2; 
                }
                popBit(tempB, blackSq);
            }
        }
        
        PTT.store(pawnKey, mgScore, egScore);
    }
   

    return ((egScore * phase) + (mgScore * (256 - phase))) / 256;
}

int EngineEval::evaluatePieceSquareTable(const Board& board, int phase){
    int mgScore = 0;
    int egScore = 0;
    uint64_t bbBoth = board.getOccupancy(BOTH);
    int count = 0;
    while(bbBoth && count <65){
        int sq = getLS1B(bbBoth);
        int pieceType = board.getPieceType(sq);
    
        if(pieceType <=5){
            mgScore+=mgTables[pieceType][sq^56];
            
            egScore+=egTables[pieceType][sq^56];
            
        }
        else{
            mgScore-=mgTables[pieceType-6][sq];
            
            egScore-=egTables[pieceType-6][sq];
        }
        count++;
        popBit(bbBoth,sq);
    }
    return (((256-phase)*mgScore) + (egScore*phase)) / 256;

}

int EngineEval::mopUpEval(const Board& board, Color winner, int phase, bool bishopKnightEndgame) {
    int egScore = 0;

    Color loser = (winner == WHITE) ? BLACK : WHITE;

    int winnerSq = board.getKingSquare(winner);
    int loserSq = board.getKingSquare(loser);

    int wf = winnerSq % 8;
    int wr = winnerSq / 8;
    int lf = loserSq % 8;
    int lr = loserSq / 8;

    int centerDist = std::max(std::abs(lf - 3), std::abs(lf - 4)) +
                     std::max(std::abs(lr - 3), std::abs(lr - 4));

    egScore += centerDist * 10;

    int kingDist = std::max(std::abs(wf - lf), std::abs(wr - lr));
    egScore += (7 - kingDist) * 10;

    if (bishopKnightEndgame) {
        uint64_t bBB = board.getPieceBB(winner == WHITE ? W_BISHOP : B_BISHOP);
        if (bBB) {
            int bSq = getLS1B(bBB);
            bool bIsWhite = isWhiteSquare(bSq);

            int corner1 = bIsWhite ? 7  : 0;  
            int corner2 = bIsWhite ? 56 : 63; 

            if(bIsWhite){
                if(loserSq == 0 || loserSq == 63) egScore-=500;
            }
            else if(!bIsWhite){
                if(loserSq == 7 || loserSq == 56) egScore -=500;
            }

            int dist1 = std::abs(lf - (corner1 % 8)) + std::abs(lr - (corner1 / 8));
            int dist2 = std::abs(lf - (corner2 % 8)) + std::abs(lr - (corner2 / 8));
            int minDistToCorrectCorner = std::min(dist1, dist2);

            egScore += (14 - minDistToCorrectCorner) * 50;
            
            if (minDistToCorrectCorner == 0) egScore += 500;
        }
    }
    
    int finalScore = (egScore * phase) / 256;

    return (winner == WHITE) ? finalScore : -finalScore;
}

int EngineEval::kingPawnStructure(const Board& board, Color color, int phase) {
    int mgScore = 0;

    int kingSq = board.getKingSquare(color);
    int kingFile = kingSq % 8;

    if (phase < 150) { 
        uint64_t myPawns = board.getPieceBB(color == WHITE ? W_PAWN : B_PAWN);
        uint64_t enemyPawns = board.getPieceBB(color == WHITE ? B_PAWN : W_PAWN);

        if (kingFile <= 2 || kingFile >= 5) {
            uint64_t kingFileMask = FileMasks[kingFile];
            
            if (!(myPawns & kingFileMask)) {
                mgScore -= 15;
                if (!(enemyPawns & kingFileMask)) mgScore -= 10; 
            }
            
            uint64_t adjMask = 0;
            if (kingFile > 0) adjMask |= FileMasks[kingFile - 1];
            if (kingFile < 7) adjMask |= FileMasks[kingFile + 1];
            
            if (!(myPawns & adjMask)) {
                mgScore -= 10; 
            }
        }
        else {
            uint64_t kingFileMask = FileMasks[kingFile];
            if (!(myPawns & kingFileMask) && !(enemyPawns & kingFileMask)) {
                mgScore -= 30; 
            }
        }

        if (color == WHITE) {
            if (kingFile >= 5) { 
                if (board.getPieceType(f2) == W_PAWN) mgScore += 20;
                else if (board.getPieceType(f3) == W_PAWN) mgScore += 10;
                else mgScore-=10;

                if (board.getPieceType(g2) == W_PAWN) mgScore += 20;
                else if (board.getPieceType(g3) == W_PAWN) mgScore += 10;
                else mgScore-=10;

                if (board.getPieceType(h2) == W_PAWN) mgScore += 20;
                else if (board.getPieceType(h3) == W_PAWN) mgScore += 10;
                else mgScore-=10;

            } else if (kingFile <= 2) { 
                if (board.getPieceType(a2) == W_PAWN) mgScore += 20;
                else if (board.getPieceType(a3) == W_PAWN) mgScore += 10;
                else mgScore-=10;

                if (board.getPieceType(b2) == W_PAWN) mgScore += 20;
                else if (board.getPieceType(b3) == W_PAWN) mgScore += 10;
                else mgScore-=10;

                if (board.getPieceType(c2) == W_PAWN) mgScore += 20;
                else if (board.getPieceType(c3) == W_PAWN) mgScore += 10;
                else mgScore-=10;
            }
        } else { 
            if (kingFile >= 5) { 
                if (board.getPieceType(f7) == B_PAWN) mgScore += 20;
                else if (board.getPieceType(f6) == B_PAWN) mgScore += 10;
                else mgScore-=10;

                if (board.getPieceType(g7) == B_PAWN) mgScore += 20;
                else if (board.getPieceType(g6) == B_PAWN) mgScore += 10;
                else mgScore-=10;

                if (board.getPieceType(h7) == B_PAWN) mgScore += 20;
                else if (board.getPieceType(h6) == B_PAWN) mgScore += 10;
                else mgScore-=10;

            } else if (kingFile <= 2) { 
                if (board.getPieceType(a7) == B_PAWN) mgScore += 20;
                else if (board.getPieceType(a6) == B_PAWN) mgScore += 10;
                else mgScore-=10;

                if (board.getPieceType(b7) == B_PAWN) mgScore += 20;
                else if (board.getPieceType(b6) == B_PAWN) mgScore += 10;
                else mgScore-=10;

                if (board.getPieceType(c7) == B_PAWN) mgScore += 20;
                else if (board.getPieceType(c6) == B_PAWN) mgScore += 10;
                else mgScore-=10;
            }
        }
    }

    return (mgScore * (256 - phase)) / 256;
}

int EngineEval::mobilityEval(const Board& board, int phase, Color color, uint64_t enemyPawnAttacks) {
    int mgScore = 0;
    int egScore = 0;

    uint64_t friends = board.getOccupancy(color);
    uint64_t occupancy = board.getOccupancy(BOTH);

    uint64_t knightBB = board.getPieceBB(color == WHITE ? W_KNIGHT : B_KNIGHT);
    while (knightBB) {
        int sq = getLS1B(knightBB);
        uint64_t freeMoves = GenerateMoves::getKnightMoves(sq) & ~friends & ~enemyPawnAttacks;
        int count = (int) __builtin_popcountll(freeMoves);

        mgScore += knightMgMobility[count];
        egScore += knightEgMobility[count];

        popBit(knightBB, sq);
    }

    uint64_t rookBB = board.getPieceBB(color == WHITE ? W_ROOK : B_ROOK);
    while (rookBB) {
        int sq = getLS1B(rookBB);
        uint64_t attacks = GenerateMoves::getRookMagicBB(sq, occupancy); 
        uint64_t freeMoves = attacks & ~friends & ~enemyPawnAttacks;
        int count = (int) __builtin_popcountll(freeMoves);

        mgScore += rookMgMobility[count];
        egScore += rookEgMobility[count];

        popBit(rookBB, sq);
    }

    uint64_t bishopBB = board.getPieceBB(color == WHITE ? W_BISHOP : B_BISHOP);
    while (bishopBB) {
        int sq = getLS1B(bishopBB);
        uint64_t attacks = GenerateMoves::getBishopMagicBB(sq, occupancy); 
        uint64_t freeMoves = attacks & ~friends & ~enemyPawnAttacks;
        int count = (int) __builtin_popcountll(freeMoves);

        mgScore += bishopMgMobility[count];
        egScore += bishopEgMobility[count];

        popBit(bishopBB, sq);
    }

    uint64_t queenBB = board.getPieceBB(color == WHITE ? W_QUEEN : B_QUEEN);
    while (queenBB) {
        int sq = getLS1B(queenBB);
        
        uint64_t queenAttacks = GenerateMoves::getRookMagicBB(sq, occupancy) | 
                                GenerateMoves::getBishopMagicBB(sq, occupancy);
        
        uint64_t freeMoves = queenAttacks & ~friends & ~enemyPawnAttacks;
        int count = (int) __builtin_popcountll(freeMoves);

        if ((color == WHITE && sq > 7) || (color==BLACK && sq<56)) { 
            if (count == 0)      { mgScore -= 150; egScore -= 100; } 
            else if (count <= 2) { mgScore -= 80;  egScore -= 40;  }  
        }
        
        popBit(queenBB, sq);
    }

    int finalScore = (((256 - phase) * mgScore) + (egScore * phase)) / 256;

    return finalScore;
}

int EngineEval::checkTrappedPatterns(const Board& board, int phase, Color color) {
    int penalty = 0;

    if (color == WHITE) {
        if (board.getPieceType(a7) == W_BISHOP && board.getPieceType(b6) == B_PAWN) penalty += 80;
        if (board.getPieceType(h7) == W_BISHOP && board.getPieceType(g6) == B_PAWN) penalty += 80;
        
        if (board.getPieceType(a6) == W_BISHOP && board.getPieceType(b7) == B_PAWN) penalty += 50;
        if (board.getPieceType(h6) == W_BISHOP && board.getPieceType(g7) == B_PAWN) penalty += 50;

        if (board.getPieceType(a8) == W_KNIGHT) penalty += 40;
        if (board.getPieceType(h8) == W_KNIGHT) penalty += 40;
        
        if (board.getPieceType(a7) == W_KNIGHT && (board.getPieceType(b6) == B_PAWN || board.getPieceType(c6) == B_PAWN)) penalty += 30;

    } else {
        if (board.getPieceType(a2) == B_BISHOP && board.getPieceType(b3) == W_PAWN) penalty += 80;
        if (board.getPieceType(h2) == B_BISHOP && board.getPieceType(g3) == W_PAWN) penalty += 80;
        
        if (board.getPieceType(a3) == B_BISHOP && board.getPieceType(b2) == W_PAWN) penalty += 50;
        if (board.getPieceType(h3) == B_BISHOP && board.getPieceType(g2) == W_PAWN) penalty += 50;
        if (board.getPieceType(a1) == B_KNIGHT) penalty += 40;
        if (board.getPieceType(h1) == B_KNIGHT) penalty += 40;
        
        if (board.getPieceType(a2) == B_KNIGHT && (board.getPieceType(b3) == W_PAWN || board.getPieceType(c3) == W_PAWN)) penalty += 30;
    }

    return (phase > 200) ? -penalty/2 : -penalty; 
}

int EngineEval::checkPinnedPieces(const Board& board, int phase) {
    int mgPenalty = 0;
    int egPenalty = 0;
    uint64_t blacks = board.getOccupancy(BLACK);
    uint64_t whites = board.getOccupancy(WHITE);
    int whiteKingSq = board.getKingSquare(WHITE);
    int blackKingSq = board.getKingSquare(BLACK);
    
    Magic wRookMagics = GenerateMoves::getRookMagic(whiteKingSq);
    uint64_t wRAttacks = wRookMagics.ptr[(blacks & wRookMagics.mask) * wRookMagics.magic >> wRookMagics.shift];
    
    Magic wBishopMagics = GenerateMoves::getBishopMagic(whiteKingSq);
    uint64_t wBAttacks = wBishopMagics.ptr[(blacks & wBishopMagics.mask) * wBishopMagics.magic >> wBishopMagics.shift];
    
    Magic bRookMagics = GenerateMoves::getRookMagic(blackKingSq);
    uint64_t bRAttacks = bRookMagics.ptr[(whites & bRookMagics.mask) * bRookMagics.magic >> bRookMagics.shift];
    
    Magic bBishopMagics = GenerateMoves::getBishopMagic(blackKingSq);
    uint64_t bBAttacks = bBishopMagics.ptr[(whites & bBishopMagics.mask) * bBishopMagics.magic >> bBishopMagics.shift];
    
    
    uint64_t wRookPinners = (wRAttacks & (board.getPieceBB(B_ROOK) | board.getPieceBB(B_QUEEN)));
    while(wRookPinners) {
        int pinnerSq = getLS1B(wRookPinners);
        popBit(wRookPinners, pinnerSq);
        
        int minSq = std::min(whiteKingSq, pinnerSq);
        int maxSq = std::max(whiteKingSq, pinnerSq);
        uint64_t boundingBoxMask = ((1ULL << maxSq) - 1) ^ ((1ULL << (minSq + 1)) - 1);
        
        uint64_t pinnerAttacks = GenerateMoves::getRookMagicBB(pinnerSq, 0ULL);
        uint64_t lineMask = (pinnerAttacks & wRAttacks) & boundingBoxMask;
        
        if(__builtin_popcountll(lineMask & whites) == 1) { 
            mgPenalty -= 20;
            egPenalty -= 42;
        }        
    }
    
    uint64_t bRookPinners = (bRAttacks & (board.getPieceBB(W_ROOK) | board.getPieceBB(W_QUEEN)));
    while(bRookPinners) {
        int pinnerSq = getLS1B(bRookPinners);
        popBit(bRookPinners, pinnerSq);
        
        int minSq = std::min(blackKingSq, pinnerSq);
        int maxSq = std::max(blackKingSq, pinnerSq);
        uint64_t boundingBoxMask = ((1ULL << maxSq) - 1) ^ ((1ULL << (minSq + 1)) - 1);
        
        uint64_t pinnerAttacks = GenerateMoves::getRookMagicBB(pinnerSq, 0ULL);
        uint64_t lineMask = (pinnerAttacks & bRAttacks) & boundingBoxMask;
        
        if(__builtin_popcountll(lineMask & blacks) == 1) { 
            mgPenalty += 20;
            egPenalty += 42;
        }        
    }
    
    uint64_t wBishopPinners = (wBAttacks & (board.getPieceBB(B_BISHOP) | board.getPieceBB(B_QUEEN)));
    while(wBishopPinners) {
        int pinnerSq = getLS1B(wBishopPinners);
        popBit(wBishopPinners, pinnerSq);
        
        int minSq = std::min(whiteKingSq, pinnerSq);
        int maxSq = std::max(whiteKingSq, pinnerSq);
        uint64_t boundingBoxMask = ((1ULL << maxSq) - 1) ^ ((1ULL << (minSq + 1)) - 1);
        
        uint64_t pinnerAttacks = GenerateMoves::getBishopMagicBB(pinnerSq, 0ULL); 
        uint64_t lineMask = (pinnerAttacks & wBAttacks) & boundingBoxMask;
        
        if(__builtin_popcountll(lineMask & whites) == 1) { 
            mgPenalty -= 20;
            egPenalty -= 42;
        }        
    }
    
    uint64_t bBishopPinners = (bBAttacks & (board.getPieceBB(W_BISHOP) | board.getPieceBB(W_QUEEN)));
    while(bBishopPinners) {
        int pinnerSq = getLS1B(bBishopPinners);
        popBit(bBishopPinners, pinnerSq);
        
        int minSq = std::min(blackKingSq, pinnerSq);
        int maxSq = std::max(blackKingSq, pinnerSq);
        uint64_t boundingBoxMask = ((1ULL << maxSq) - 1) ^ ((1ULL << (minSq + 1)) - 1);
        
        uint64_t pinnerAttacks = GenerateMoves::getBishopMagicBB(pinnerSq, 0ULL); 
        uint64_t lineMask = (pinnerAttacks & bBAttacks) & boundingBoxMask;
        
        if(__builtin_popcountll(lineMask & blacks) == 1) { 
            mgPenalty += 20;
            egPenalty += 42;
        }        
    }
    
    
    return (((256 - phase) * mgPenalty) + (phase * egPenalty)) / 256;
}

int EngineEval::kingSafetyEval(const Board& board, int phase) {
    int dangerToWhite = 0; 
    int dangerToBlack = 0; 
    
    uint64_t blacks = board.getOccupancy(BLACK);
    uint64_t whites = board.getOccupancy(WHITE);
    uint64_t occupancy = board.getOccupancy(BOTH);
    
    uint64_t wKingZone = kingNearSq[0][board.getKingSquare(WHITE)];
    uint64_t bKingZone = kingNearSq[1][board.getKingSquare(BLACK)];
    
    uint64_t wRooks = board.getPieceBB(W_ROOK);
    uint64_t wBishops = board.getPieceBB(W_BISHOP);
    uint64_t wQueens = board.getPieceBB(W_QUEEN);
    uint64_t wKnights = board.getPieceBB(W_KNIGHT);
    
    while (wRooks) {
        int sq = getLS1B(wRooks);
        uint64_t attacks = GenerateMoves::getRookMagicBB(sq, occupancy) & ~whites;
        dangerToBlack += 3 * (int) __builtin_popcountll(attacks & bKingZone);
        popBit(wRooks, sq);
    }
    while (wBishops) {
        int sq = getLS1B(wBishops);
        uint64_t attacks = GenerateMoves::getBishopMagicBB(sq, occupancy) & ~whites;
        dangerToBlack += 2 * (int) __builtin_popcountll(attacks & bKingZone);
        popBit(wBishops, sq);
    }
    while (wQueens) {
        int sq = getLS1B(wQueens);
        uint64_t attacks = (GenerateMoves::getRookMagicBB(sq, occupancy) | 
        GenerateMoves::getBishopMagicBB(sq, occupancy)) & ~whites;
        dangerToBlack += 5 * (int) __builtin_popcountll(attacks & bKingZone);
        popBit(wQueens, sq);
    }
    while (wKnights) {
        int sq = getLS1B(wKnights);
        uint64_t attacks = GenerateMoves::getKnightMoves(sq) & ~whites;
        dangerToBlack += 2 * (int) __builtin_popcountll(attacks & bKingZone);
        popBit(wKnights, sq);
    }
    
    uint64_t bRooks = board.getPieceBB(B_ROOK);
    uint64_t bBishops = board.getPieceBB(B_BISHOP);
    uint64_t bQueens = board.getPieceBB(B_QUEEN);
    uint64_t bKnights = board.getPieceBB(B_KNIGHT);
    
    while (bRooks) {
        int sq = getLS1B(bRooks);
        uint64_t attacks = GenerateMoves::getRookMagicBB(sq, occupancy) & ~blacks;
        dangerToWhite += 3 * (int) __builtin_popcountll(attacks & wKingZone);
        popBit(bRooks, sq);
    }
    while (bBishops) {
        int sq = getLS1B(bBishops);
        uint64_t attacks = GenerateMoves::getBishopMagicBB(sq, occupancy) & ~blacks;
        dangerToWhite += 2 * (int) __builtin_popcountll(attacks & wKingZone);
        popBit(bBishops, sq);
    }
    while (bQueens) {
        int sq = getLS1B(bQueens);
        uint64_t attacks = (GenerateMoves::getRookMagicBB(sq, occupancy) | 
        GenerateMoves::getBishopMagicBB(sq, occupancy)) & ~blacks;
        dangerToWhite += 5 * (int) __builtin_popcountll(attacks & wKingZone);
        popBit(bQueens, sq);
    }
    while (bKnights) {
        int sq = getLS1B(bKnights);
        uint64_t attacks = GenerateMoves::getKnightMoves(sq) & ~blacks;
        dangerToWhite += 2 * (int) __builtin_popcountll(attacks & wKingZone);
        popBit(bKnights, sq);
    }
    
    int overallScore = SafetyTable[dangerToBlack] - SafetyTable[dangerToWhite];
    
    
    return (overallScore * (256 - phase)) / 256; 
}

int EngineEval::underDevelopmentPenalty(const Board& board, int phase){
    int mgScore = 0;
    int whiteDevPenalty = 0;
    int blackDevPenalty = 0;
    if(phase < 128){
        uint64_t wKnights = board.getPieceBB(W_KNIGHT);
        uint64_t wBishops = board.getPieceBB(W_BISHOP);
        if (wKnights & (1ULL << 1))  whiteDevPenalty += 15; 
        if (wKnights & (1ULL << 6))  whiteDevPenalty += 15; 
        if (wBishops & (1ULL << 2))  whiteDevPenalty += 15; 
        if (wBishops & (1ULL << 5))  whiteDevPenalty += 15; 
        
        uint64_t bKnights = board.getPieceBB(B_KNIGHT);
        uint64_t bBishops = board.getPieceBB(B_BISHOP);
        if (bKnights & (1ULL << 57)) blackDevPenalty += 15; 
        if (bKnights & (1ULL << 62)) blackDevPenalty += 15; 
        if (bBishops & (1ULL << 58)) blackDevPenalty += 15; 
        if (bBishops & (1ULL << 61)) blackDevPenalty += 15; 
    }
    
    mgScore -= whiteDevPenalty;
    mgScore += blackDevPenalty;
    
    return ((256-phase)*mgScore)/256;
}

int EngineEval::threatEval(const Board& board, int phase, uint64_t whitePawnAttacks, uint64_t blackPawnAttacks){
    int mgScore = 0;
    int egScore = 0; 
    uint64_t wQueen  = board.getPieceBB(W_QUEEN);
    uint64_t wRooks  = board.getPieceBB(W_ROOK);
    uint64_t wMinors = board.getPieceBB(W_KNIGHT) | board.getPieceBB(W_BISHOP);
    
    uint64_t bQueen  = board.getPieceBB(B_QUEEN);
    uint64_t bRooks  = board.getPieceBB(B_ROOK);
    uint64_t bMinors = board.getPieceBB(B_KNIGHT) | board.getPieceBB(B_BISHOP);
    
    int wPawnThreatsQ = (int) __builtin_popcountll(whitePawnAttacks & bQueen);
    int wPawnThreatsR = (int) __builtin_popcountll(whitePawnAttacks & bRooks);
    int wPawnThreatsM = (int) __builtin_popcountll(whitePawnAttacks & bMinors);
    
    mgScore += (wPawnThreatsQ * 50) + (wPawnThreatsR * 30) + (wPawnThreatsM * 15);
    egScore += (wPawnThreatsQ * 40) + (wPawnThreatsR * 25) + (wPawnThreatsM * 15);
    
    int bPawnThreatsQ = (int) __builtin_popcountll(blackPawnAttacks & wQueen);
    int bPawnThreatsR = (int) __builtin_popcountll(blackPawnAttacks & wRooks);
    int bPawnThreatsM = (int) __builtin_popcountll(blackPawnAttacks & wMinors);
    
    mgScore -= (bPawnThreatsQ * 50) + (bPawnThreatsR * 30) + (bPawnThreatsM * 15);
    egScore -= (bPawnThreatsQ * 40) + (bPawnThreatsR * 25) + (bPawnThreatsM * 15);
    
    uint64_t occ = board.getOccupancy(BOTH);
    
    uint64_t wMinorAttacks = 0;
    uint64_t tempWKnights = board.getPieceBB(W_KNIGHT);
    while (tempWKnights) {
        int sq = getLS1B(tempWKnights);
        wMinorAttacks |= GenerateMoves::getKnightMoves(sq); 
        popBit(tempWKnights, sq);
    }
    uint64_t tempWBishops = board.getPieceBB(W_BISHOP);
    while (tempWBishops) {
        int sq = getLS1B(tempWBishops);
        wMinorAttacks |= GenerateMoves::getBishopMagicBB(sq, occ); 
        popBit(tempWBishops, sq);
    }
    
    int wMinorThreatsQ = (int) __builtin_popcountll(wMinorAttacks & bQueen);
    int wMinorThreatsR = (int) __builtin_popcountll(wMinorAttacks & bRooks);
    mgScore += (wMinorThreatsQ * 35) + (wMinorThreatsR * 15);
    egScore += (wMinorThreatsQ * 25) + (wMinorThreatsR * 10);
    
    uint64_t bMinorAttacks = 0;
    uint64_t tempBKnights = board.getPieceBB(B_KNIGHT);
    while (tempBKnights) {
        int sq = getLS1B(tempBKnights);
        bMinorAttacks |= GenerateMoves::getKnightMoves(sq); 
        popBit(tempBKnights, sq);
    }
    uint64_t tempBBishops = board.getPieceBB(B_BISHOP);
    while (tempBBishops) {
        int sq = getLS1B(tempBBishops);
        bMinorAttacks |= GenerateMoves::getBishopMagicBB(sq, occ); 
        popBit(tempBBishops, sq);
    }
    
    int bMinorThreatsQ = (int) __builtin_popcountll(bMinorAttacks & wQueen);
    int bMinorThreatsR = (int) __builtin_popcountll(bMinorAttacks & wRooks);
    mgScore -= (bMinorThreatsQ * 35) + (bMinorThreatsR * 15);
    egScore -= (bMinorThreatsQ * 25) + (bMinorThreatsR * 10);
    
    return (((256 - phase) * mgScore) + (phase * egScore)) / 256;
}

bool EngineEval::isEndGame(const Board& board){
    int phase = taperedEval(board);
    if(phase > 154) return true;
    return false;
}

int EngineEval::getPhase(const Board& board){
    return taperedEval(board);
}

uint64_t EngineEval::enemyPawnAttacks(const Board& board, Color enemyColor) {
    uint64_t pawns = board.getPieceBB(enemyColor == WHITE ? W_PAWN : B_PAWN);
    
    if (enemyColor == WHITE) {
        return ((pawns &~FILE_A) << 7)  | ((pawns & ~FILE_H) << 9);
    } else {
        return ((pawns & ~FILE_H) >> 7) | ((pawns & ~FILE_A) >> 9 );
    }
}