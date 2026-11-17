#include <stdint.h>
#include "Types.h"
#include <string>
#include "Board.h"
#include "EngineEval.h"
#include "Constants.h"
#include <iostream>
#include <algorithm>
#include "GenerateMoves.h"
#include "TransPositionTable.h"
#include "EvalParams.h"
using namespace Constants;

uint64_t FILE_H = 0x8080808080808080ULL;
uint64_t FILE_A = 0x0101010101010101ULL;

// Shorthand for the tunable weights (see EvalParams.h).
static const EvalParams& P = evalParams;


int EngineEval::evaluate(const Board& board){
    int score = 0;
    if(board.sideToMove == WHITE) score+=P.tempo;
    else score-=P.tempo;
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
        if((wKnight == 2 && blackPieces == 1) || (bKnight == 2 && whitePieces == 1)) return 0;
        else if(wKnight == 1 && wBishop == 1 && blackPieces == 1) bishopKnightEndgame = true;
        else if(bKnight == 1 && bBishop == 1 && whitePieces == 1) bishopKnightEndgame = true;
    }
    int phase = taperedEval(board);
    
    int materialDiff=pureEvaluateMaterial(board,phase);
    
    score+= evaluateMaterial(board, phase);
    
    score+=evaluatePawnStructure(board,phase,whitePawnAttacks,blackPawnAttacks);
    
    score+=bishopPair(board,phase);

    score+=evaluatePieceSquareTable(board,phase);

    if(phase >150 && materialDiff != 0){
        Color winner = (materialDiff > 0) ? WHITE : BLACK;
        int mopUp=mopUpEval(board, winner, phase, bishopKnightEndgame);
        score += mopUp;
    }
    
    score+=kingPawnStructure(board,WHITE,phase) - kingPawnStructure(board,BLACK,phase);

    score+= checkTrappedPatterns(board, phase, WHITE) - checkTrappedPatterns(board, phase, BLACK);

    uint64_t whiteKingZone = kingNearSq[0][board.getKingSquare(WHITE)];
    uint64_t blackKingZone = kingNearSq[1][board.getKingSquare(BLACK)];

    int whiteMgMobility = 0, whiteEgMobility = 0, dangerToBlack = 0;
    int blackMgMobility = 0, blackEgMobility = 0, dangerToWhite = 0;

    AttackInfo whiteInfo, blackInfo;
    uint64_t whiteMinorAttacks = evaluatePieceActivity(board, WHITE, blackPawnAttacks, blackKingZone, whiteMgMobility, whiteEgMobility, dangerToBlack, whiteInfo);
    uint64_t blackMinorAttacks = evaluatePieceActivity(board, BLACK, whitePawnAttacks, whiteKingZone, blackMgMobility, blackEgMobility, dangerToWhite, blackInfo);

    int whiteMobility = (((256 - phase) * whiteMgMobility) + (whiteEgMobility * phase)) / 256;
    int blackMobility = (((256 - phase) * blackMgMobility) + (blackEgMobility * phase)) / 256;
    score += whiteMobility - blackMobility;

    score+= checkPinnedPieces(board,phase);

    // Everything each side attacks, pawns and king included.
    uint64_t whiteAttacks = whiteInfo.all | whitePawnAttacks | kingAttacks(board.getKingSquare(WHITE));
    uint64_t blackAttacks = blackInfo.all | blackPawnAttacks | kingAttacks(board.getKingSquare(BLACK));

    // King zone danger counts for less with few attackers or without a queen;
    // safe checks are added on top.
    dangerToBlack = dangerToBlack * P.kingAttackerScale[std::min(whiteInfo.kingZoneAttackers, 4)] / 100;
    dangerToWhite = dangerToWhite * P.kingAttackerScale[std::min(blackInfo.kingZoneAttackers, 4)] / 100;
    if (!board.getPieceBB(W_QUEEN)) dangerToBlack = dangerToBlack * P.kingDangerNoQueen / 100;
    if (!board.getPieceBB(B_QUEEN)) dangerToWhite = dangerToWhite * P.kingDangerNoQueen / 100;
    dangerToBlack += safeCheckDanger(board, BLACK, whiteInfo, blackAttacks);
    dangerToWhite += safeCheckDanger(board, WHITE, blackInfo, whiteAttacks);
    dangerToBlack = std::max(dangerToBlack, 0);
    dangerToWhite = std::max(dangerToWhite, 0);

    score+= kingSafetyEval(phase, dangerToBlack, dangerToWhite);

    score+= passedPawnDynamic(board, phase, whiteAttacks, blackAttacks);

    // score+=underDevelopmentPenalty(board,phase);

    score+=threatEval(board,phase,whitePawnAttacks,blackPawnAttacks,whiteMinorAttacks,blackMinorAttacks);

    score+=outpostEval(board,phase,whitePawnAttacks,blackPawnAttacks);

    score = scaleEndgame(board, phase, score);

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

    mgScore += (wP - bP) * P.material[0].mg;
    mgScore += (wN - bN) * P.material[1].mg;
    mgScore += (wB - bB) * P.material[2].mg;
    mgScore += (wR - bR) * P.material[3].mg;
    mgScore += (wQ - bQ) * P.material[4].mg;

    egScore += (wP - bP) * P.material[0].eg;
    egScore += (wN - bN) * P.material[1].eg;
    egScore += (wB - bB) * P.material[2].eg;
    egScore += (wR - bR) * P.material[3].eg;
    egScore += (wQ - bQ) * P.material[4].eg;

    // Imbalance: piece values that depend on the rest of the material.
    int knightPawns = wN * (wP - 5) - bN * (bP - 5);
    int rookPawns   = wR * (wP - 5) - bR * (bP - 5);
    mgScore += knightPawns * P.knightPawnAdjust.mg + rookPawns * P.rookPawnAdjust.mg;
    egScore += knightPawns * P.knightPawnAdjust.eg + rookPawns * P.rookPawnAdjust.eg;

    int minorDiff = (wN + wB) - (bN + bB);
    int rookDiff  = wR - bR;
    int minorsForRook = 0;   // > 0: White has the minors, Black the rook
    if (minorDiff > 0 && rookDiff < 0) minorsForRook = std::min(minorDiff, -rookDiff);
    else if (minorDiff < 0 && rookDiff > 0) minorsForRook = -std::min(-minorDiff, rookDiff);
    mgScore += minorsForRook * P.minorsForRook.mg;
    egScore += minorsForRook * P.minorsForRook.eg;

    return (((256-phase)*mgScore) + (egScore*phase)) / 256;
}

// Raw middlegame material balance. Uses the fixed VAL_* values on purpose: it only
// decides which side the mop-up evaluation helps, and should not move with tuning.
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
        mgBonus += P.bishopPair.mg;
        egBonus += P.bishopPair.eg;
    }
    if (__builtin_popcountll(board.getPieceBB(B_BISHOP)) >= 2) {
        mgBonus -= P.bishopPair.mg;
        egBonus -= P.bishopPair.eg;
    }

    int finalBonus = ((mgBonus * (256 - phase)) + (egBonus * phase)) / 256;
    
    return finalBonus;
}


int EngineEval::evaluatePawnStructure(const Board& board, int phase, uint64_t whitePawnAttacks, uint64_t blackPawnAttacks) {
    int mgScore = 0, egScore = 0;
    uint64_t pawnKey = board.getPawnKey();
    PawnEntry* entry = usePawnHash ? PTT.pawnProbe(pawnKey) : nullptr;

    if (entry != nullptr && entry->key == pawnKey) {
        return ((entry->egScore * phase) + (entry->mgScore * (256 - phase))) / 256;
    }

    uint64_t whitePawns = board.getPieceBB(W_PAWN);
    uint64_t blackPawns = board.getPieceBB(B_PAWN);

    if (whitePawns & (1ULL << e4)) { mgScore += P.centerPawn.mg; egScore += P.centerPawn.eg; }
    if (whitePawns & (1ULL << d4)) { mgScore += P.centerPawn.mg; egScore += P.centerPawn.eg; }
    if (blackPawns & (1ULL << e5)) { mgScore -= P.centerPawn.mg; egScore -= P.centerPawn.eg; }
    if (blackPawns & (1ULL << d5)) { mgScore -= P.centerPawn.mg; egScore -= P.centerPawn.eg; }

    if ((whitePawns & (1ULL << e4)) && (whitePawns & (1ULL << d4))) { mgScore += P.centerPawnDuo.mg; egScore += P.centerPawnDuo.eg; }
    if ((blackPawns & (1ULL << e5)) && (blackPawns & (1ULL << d5))) { mgScore -= P.centerPawnDuo.mg; egScore -= P.centerPawnDuo.eg; }

    for (int i = 0; i < 8; i++) {
        uint64_t wPawnsOnFile = whitePawns & FileMasks[i];
        uint64_t bPawnsOnFile = blackPawns & FileMasks[i];

        if (wPawnsOnFile & (wPawnsOnFile - 1)) { mgScore += P.doubledPawn.mg; egScore += P.doubledPawn.eg; }
        if (bPawnsOnFile & (bPawnsOnFile - 1)) { mgScore -= P.doubledPawn.mg; egScore -= P.doubledPawn.eg; }

        uint64_t adj = 0;
        if (i > 0) adj |= FileMasks[i - 1];
        if (i < 7) adj |= FileMasks[i + 1];

        bool wIsolated = !(whitePawns & adj);
        bool bIsolated = !(blackPawns & adj);

        uint64_t tempW = wPawnsOnFile;
        while (tempW) {
            int whiteSq = getLS1B(tempW);
            int rank = whiteSq / 8;
            uint64_t bit = 1ULL << whiteSq;

            if (wIsolated) {
                mgScore += P.isolatedPawn.mg;
                egScore += P.isolatedPawn.eg;
            }

            uint64_t neighbours = ((bit & ~FILE_A) >> 1) | ((bit & ~FILE_H) << 1);
            if ((whitePawnAttacks & bit) || (whitePawns & neighbours)) {
                mgScore += P.connectedPawnMg[rank];
                egScore += P.connectedPawnEg[rank];
            }
            // Backward: no own pawn on an adjacent file at this rank or behind it.
            else if (!wIsolated && rank < 7 && !(whitePawns & adj & PassedPawnMasks[BLACK][whiteSq + 8])
                     && (blackPawnAttacks & (bit << 8))) {
                mgScore += P.backwardPawn.mg;
                egScore += P.backwardPawn.eg;
                if (!(blackPawns & FileMasks[i])) {
                    mgScore += P.backwardOpenFile.mg;
                    egScore += P.backwardOpenFile.eg;
                }
            }

            if (!(blackPawns & PassedPawnMasks[WHITE][whiteSq])) {
                mgScore += P.passedPawnMg[rank];
                egScore += P.passedPawnEg[rank];

                if (whitePawnAttacks & (1ULL << whiteSq)) {
                    mgScore += P.protectedPassedPawn.mg;
                    egScore += P.protectedPassedPawn.eg;
                }
            }

            else if (rank < 6 && !(blackPawns & PassedPawnMasks[WHITE][whiteSq + 8])
                              && !(blackPawns & (1ULL << (whiteSq + 8))))
            {
                int nextRank = rank + 1;
                mgScore += P.passedPawnMg[nextRank] / 2;
                egScore += P.passedPawnEg[nextRank] / 2;
            }

            popBit(tempW, whiteSq);
        }

        uint64_t tempB = bPawnsOnFile;
        while (tempB) {
            int blackSq = getLS1B(tempB);
            int rank = blackSq / 8;
            uint64_t bit = 1ULL << blackSq;

            // İzole Piyon Cezası
            if (bIsolated) {
                mgScore -= P.isolatedPawn.mg;
                egScore -= P.isolatedPawn.eg;
            }

            uint64_t neighbours = ((bit & ~FILE_A) >> 1) | ((bit & ~FILE_H) << 1);
            if ((blackPawnAttacks & bit) || (blackPawns & neighbours)) {
                mgScore -= P.connectedPawnMg[7 - rank];
                egScore -= P.connectedPawnEg[7 - rank];
            }
            else if (!bIsolated && rank > 0 && !(blackPawns & adj & PassedPawnMasks[WHITE][blackSq - 8])
                     && (whitePawnAttacks & (bit >> 8))) {
                mgScore -= P.backwardPawn.mg;
                egScore -= P.backwardPawn.eg;
                if (!(whitePawns & FileMasks[i])) {
                    mgScore -= P.backwardOpenFile.mg;
                    egScore -= P.backwardOpenFile.eg;
                }
            }

            if (!(whitePawns & PassedPawnMasks[BLACK][blackSq])) {
                mgScore -= P.passedPawnMg[7 - rank];
                egScore -= P.passedPawnEg[7 - rank];

                if (blackPawnAttacks & (1ULL << blackSq)) {
                    mgScore -= P.protectedPassedPawn.mg;
                    egScore -= P.protectedPassedPawn.eg;
                }
            }
            // Aday Geçmiş Piyon Kontrolü
            else if (rank > 1 && !(whitePawns & PassedPawnMasks[BLACK][blackSq - 8])
                             && !(whitePawns & (1ULL << (blackSq - 8))))
            {
                int nextRank = rank - 1;
                mgScore -= P.passedPawnMg[7 - nextRank] / 2;
                egScore -= P.passedPawnEg[7 - nextRank] / 2;
            }

            popBit(tempB, blackSq);
        }
    }

    if (usePawnHash) PTT.store(pawnKey, mgScore, egScore);
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
            mgScore+=P.pstMg[pieceType][sq^56];
            
            egScore+=P.pstEg[pieceType][sq^56];
            
        }
        else{
            mgScore-=P.pstMg[pieceType-6][sq];
            
            egScore-=P.pstEg[pieceType-6][sq];
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
    
    int rampedPhase = phase - 150;
    if (rampedPhase < 0) rampedPhase = 0;
    int finalScore = (egScore * rampedPhase) / (256 - 150);

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
                mgScore += P.kingFileNoOwnPawn;
                if (!(enemyPawns & kingFileMask)) mgScore += P.kingFileFullyOpen;
            }
            
            uint64_t adjMask = 0;
            if (kingFile > 0) adjMask |= FileMasks[kingFile - 1];
            if (kingFile < 7) adjMask |= FileMasks[kingFile + 1];
            
            if (!(myPawns & adjMask)) {
                mgScore += P.kingAdjFilesNoOwnPawn;
            }
        }
        else {
            uint64_t kingFileMask = FileMasks[kingFile];
            if (!(myPawns & kingFileMask) && !(enemyPawns & kingFileMask)) {
                mgScore += P.kingCenterOpenFile;
            }
        }

        if (color == WHITE) {
            if (kingFile >= 5) { 
                if (board.getPieceType(f2) == W_PAWN) mgScore += P.shieldPawnUnmoved;
                else if (board.getPieceType(f3) == W_PAWN) mgScore += P.shieldPawnAdvanced;
                else mgScore += P.shieldPawnMissing;

                if (board.getPieceType(g2) == W_PAWN) mgScore += P.shieldPawnUnmoved;
                else if (board.getPieceType(g3) == W_PAWN) mgScore += P.shieldPawnAdvanced;
                else mgScore += P.shieldPawnMissing;

                if (board.getPieceType(h2) == W_PAWN) mgScore += P.shieldPawnUnmoved;
                else if (board.getPieceType(h3) == W_PAWN) mgScore += P.shieldPawnAdvanced;
                else mgScore += P.shieldPawnMissing;

            } else if (kingFile <= 2) { 
                if (board.getPieceType(a2) == W_PAWN) mgScore += P.shieldPawnUnmoved;
                else if (board.getPieceType(a3) == W_PAWN) mgScore += P.shieldPawnAdvanced;
                else mgScore += P.shieldPawnMissing;

                if (board.getPieceType(b2) == W_PAWN) mgScore += P.shieldPawnUnmoved;
                else if (board.getPieceType(b3) == W_PAWN) mgScore += P.shieldPawnAdvanced;
                else mgScore += P.shieldPawnMissing;

                if (board.getPieceType(c2) == W_PAWN) mgScore += P.shieldPawnUnmoved;
                else if (board.getPieceType(c3) == W_PAWN) mgScore += P.shieldPawnAdvanced;
                else mgScore += P.shieldPawnMissing;
            }
        } else { 
            if (kingFile >= 5) { 
                if (board.getPieceType(f7) == B_PAWN) mgScore += P.shieldPawnUnmoved;
                else if (board.getPieceType(f6) == B_PAWN) mgScore += P.shieldPawnAdvanced;
                else mgScore += P.shieldPawnMissing;

                if (board.getPieceType(g7) == B_PAWN) mgScore += P.shieldPawnUnmoved;
                else if (board.getPieceType(g6) == B_PAWN) mgScore += P.shieldPawnAdvanced;
                else mgScore += P.shieldPawnMissing;

                if (board.getPieceType(h7) == B_PAWN) mgScore += P.shieldPawnUnmoved;
                else if (board.getPieceType(h6) == B_PAWN) mgScore += P.shieldPawnAdvanced;
                else mgScore += P.shieldPawnMissing;

            } else if (kingFile <= 2) { 
                if (board.getPieceType(a7) == B_PAWN) mgScore += P.shieldPawnUnmoved;
                else if (board.getPieceType(a6) == B_PAWN) mgScore += P.shieldPawnAdvanced;
                else mgScore += P.shieldPawnMissing;

                if (board.getPieceType(b7) == B_PAWN) mgScore += P.shieldPawnUnmoved;
                else if (board.getPieceType(b6) == B_PAWN) mgScore += P.shieldPawnAdvanced;
                else mgScore += P.shieldPawnMissing;

                if (board.getPieceType(c7) == B_PAWN) mgScore += P.shieldPawnUnmoved;
                else if (board.getPieceType(c6) == B_PAWN) mgScore += P.shieldPawnAdvanced;
                else mgScore += P.shieldPawnMissing;
            }
        }

        // Pawn storm: on the king's file and its neighbours, the enemy pawn that
        // has come closest to our side of the board.
        for (int f = std::max(0, kingFile - 1); f <= std::min(7, kingFile + 1); f++) {
            uint64_t stormers = enemyPawns & FileMasks[f];
            if (!stormers) continue;
            int sq = (color == WHITE) ? getLS1B(stormers) : 63 - __builtin_clzll(stormers);
            int rank = (color == WHITE) ? sq / 8 : 7 - sq / 8;   // seen from the defender
            int inFront = (color == WHITE) ? sq - 8 : sq + 8;
            if (myPawns & (1ULL << inFront)) mgScore += P.pawnStormBlocked[rank];
            else mgScore += P.pawnStorm[rank];
        }
    }

    return (mgScore * (256 - phase)) / 256;
}

uint64_t EngineEval::evaluatePieceActivity(const Board& board, Color color, uint64_t enemyPawnAttacks, uint64_t enemyKingZone, int& mgMobility, int& egMobility, int& kingDanger, AttackInfo& info) {
    uint64_t friends = board.getOccupancy(color);
    uint64_t occupancy = board.getOccupancy(BOTH);
    uint64_t minorAttacks = 0;
    uint64_t ownPawns   = board.getPieceBB(color == WHITE ? W_PAWN : B_PAWN);
    uint64_t enemyPawns = board.getPieceBB(color == WHITE ? B_PAWN : W_PAWN);

    uint64_t knightBB = board.getPieceBB(color == WHITE ? W_KNIGHT : B_KNIGHT);
    while (knightBB) {
        int sq = getLS1B(knightBB);
        uint64_t attacks = GenerateMoves::getKnightMoves(sq);
        minorAttacks |= attacks;
        info.knights |= attacks;
        if (attacks & enemyKingZone) info.kingZoneAttackers++;

        uint64_t freeMoves = attacks & ~friends & ~enemyPawnAttacks;
        int count = (int) __builtin_popcountll(freeMoves);
        mgMobility += P.knightMobilityMg[count];
        egMobility += P.knightMobilityEg[count];

        kingDanger += P.kingAttackKnight * (int) __builtin_popcountll((attacks & ~friends) & enemyKingZone);

        popBit(knightBB, sq);
    }

    // 7th rank from this side's point of view, and the enemy king's back rank.
    int seventh = (color == WHITE) ? 6 : 1;
    int enemyBackRank = (color == WHITE) ? 7 : 0;
    int enemyKingRank = board.getKingSquare(color == WHITE ? BLACK : WHITE) / 8;

    uint64_t rookBB = board.getPieceBB(color == WHITE ? W_ROOK : B_ROOK);
    while (rookBB) {
        int sq = getLS1B(rookBB);
        uint64_t attacks = GenerateMoves::getRookMagicBB(sq, occupancy);
        info.rooks |= attacks;
        if (attacks & enemyKingZone) info.kingZoneAttackers++;

        uint64_t freeMoves = attacks & ~friends & ~enemyPawnAttacks;
        int count = (int) __builtin_popcountll(freeMoves);
        mgMobility += P.rookMobilityMg[count];
        egMobility += P.rookMobilityEg[count];

        uint64_t file = FileMasks[sq % 8];
        if (!(ownPawns & file)) {
            if (!(enemyPawns & file)) { mgMobility += P.rookOpenFile.mg;     egMobility += P.rookOpenFile.eg; }
            else                      { mgMobility += P.rookSemiOpenFile.mg; egMobility += P.rookSemiOpenFile.eg; }
        }
        if (sq / 8 == seventh && ((enemyPawns & (0xFFULL << (8 * seventh))) || enemyKingRank == enemyBackRank)) {
            mgMobility += P.rookOn7th.mg;
            egMobility += P.rookOn7th.eg;
        }

        kingDanger += P.kingAttackRook * (int) __builtin_popcountll((attacks & ~friends) & enemyKingZone);

        popBit(rookBB, sq);
    }

    uint64_t bishopBB = board.getPieceBB(color == WHITE ? W_BISHOP : B_BISHOP);
    while (bishopBB) {
        int sq = getLS1B(bishopBB);
        uint64_t attacks = GenerateMoves::getBishopMagicBB(sq, occupancy);
        minorAttacks |= attacks;
        info.bishops |= attacks;
        if (attacks & enemyKingZone) info.kingZoneAttackers++;

        uint64_t freeMoves = attacks & ~friends & ~enemyPawnAttacks;
        int count = (int) __builtin_popcountll(freeMoves);
        mgMobility += P.bishopMobilityMg[count];
        egMobility += P.bishopMobilityEg[count];

        kingDanger += P.kingAttackBishop * (int) __builtin_popcountll((attacks & ~friends) & enemyKingZone);

        popBit(bishopBB, sq);
    }

    uint64_t queenBB = board.getPieceBB(color == WHITE ? W_QUEEN : B_QUEEN);
    while (queenBB) {
        int sq = getLS1B(queenBB);

        uint64_t attacks = GenerateMoves::getRookMagicBB(sq, occupancy) |
                            GenerateMoves::getBishopMagicBB(sq, occupancy);
        info.queens |= attacks;
        if (attacks & enemyKingZone) info.kingZoneAttackers++;

        uint64_t freeMoves = attacks & ~friends & ~enemyPawnAttacks;
        int count = (int) __builtin_popcountll(freeMoves);

        if ((color == WHITE && sq > 7) || (color==BLACK && sq<56)) {
            if (count <= 2) { mgMobility += P.queenFewMoves.mg; egMobility += P.queenFewMoves.eg; }
        }

        kingDanger += P.kingAttackQueen * (int) __builtin_popcountll((attacks & ~friends) & enemyKingZone);

        popBit(queenBB, sq);
    }

    info.all = info.knights | info.bishops | info.rooks | info.queens;
    return minorAttacks;
}

uint64_t EngineEval::kingAttacks(int sq) {
    uint64_t b = 1ULL << sq;
    uint64_t sides = ((b & ~FILE_A) >> 1) | ((b & ~FILE_H) << 1);
    uint64_t row = b | sides;
    return sides | (row << 8) | (row >> 8);
}

// Danger (in kingSafetyTable units) from squares where the attacker's pieces
// could give check without being taken: not attacked by the defender (pieces,
// pawns or king) and not occupied by the attacker's own pieces.
int EngineEval::safeCheckDanger(const Board& board, Color defender, const AttackInfo& attacker, uint64_t defenderAttacks) {
    int kingSq = board.getKingSquare(defender);
    uint64_t occupancy = board.getOccupancy(BOTH);
    uint64_t safe = ~defenderAttacks & ~board.getOccupancy(defender == WHITE ? BLACK : WHITE);
    uint64_t diagonal = GenerateMoves::getBishopMagicBB(kingSq, occupancy);
    uint64_t straight = GenerateMoves::getRookMagicBB(kingSq, occupancy);

    int danger = 0;
    danger += P.safeCheckKnight * (int) __builtin_popcountll(GenerateMoves::getKnightMoves(kingSq) & attacker.knights & safe);
    danger += P.safeCheckBishop * (int) __builtin_popcountll(diagonal & attacker.bishops & safe);
    danger += P.safeCheckRook   * (int) __builtin_popcountll(straight & attacker.rooks & safe);
    danger += P.safeCheckQueen  * (int) __builtin_popcountll((diagonal | straight) & attacker.queens & safe);
    return danger;
}

// Passed pawn terms that depend on the kings and pieces, so they cannot live in
// the pawn hash: king distances, blocked or free way to promotion, and pawn
// endgames the defending king can no longer save. Endgame only; White minus Black.
int EngineEval::passedPawnDynamic(const Board& board, int phase, uint64_t whiteAttacks, uint64_t blackAttacks) {
    uint64_t whitePawns = board.getPieceBB(W_PAWN);
    uint64_t blackPawns = board.getPieceBB(B_PAWN);
    uint64_t occupancy = board.getOccupancy(BOTH);
    int wk = board.getKingSquare(WHITE);
    int bk = board.getKingSquare(BLACK);
    auto dist = [](int a, int b) { return std::max(std::abs(a % 8 - b % 8), std::abs(a / 8 - b / 8)); };
    auto hasPieces = [&](Color c) {
        return (board.getPieceBB(c == WHITE ? W_KNIGHT : B_KNIGHT) | board.getPieceBB(c == WHITE ? W_BISHOP : B_BISHOP) |
                board.getPieceBB(c == WHITE ? W_ROOK : B_ROOK) | board.getPieceBB(c == WHITE ? W_QUEEN : B_QUEEN)) != 0;
    };
    bool whiteHasPieces = hasPieces(WHITE);
    bool blackHasPieces = hasPieces(BLACK);
    int egScore = 0;

    uint64_t pawns = whitePawns;
    while (pawns) {
        int sq = getLS1B(pawns);
        popBit(pawns, sq);
        if (blackPawns & PassedPawnMasks[WHITE][sq]) continue;
        int r = sq / 8;              // relative rank
        int stop = sq + 8;
        uint64_t path = PassedPawnMasks[WHITE][sq] & FileMasks[sq % 8];
        egScore += P.passedOwnKingDist[r] * dist(wk, stop) + P.passedEnemyKingDist[r] * dist(bk, stop);
        if (occupancy & (1ULL << stop)) egScore += P.passedBlocked[r];
        else if (!(path & (occupancy | blackAttacks))) egScore += P.passedFreePath[r];
        if (!blackHasPieces && !(path & occupancy)) {
            int promo = 56 + sq % 8;
            int pawnDist = (r == 1) ? 5 : 7 - r;
            int kingDist = dist(bk, promo) - (board.sideToMove == BLACK ? 1 : 0);
            if (kingDist > pawnDist) egScore += P.unstoppablePasser;
        }
    }

    pawns = blackPawns;
    while (pawns) {
        int sq = getLS1B(pawns);
        popBit(pawns, sq);
        if (whitePawns & PassedPawnMasks[BLACK][sq]) continue;
        int r = 7 - sq / 8;          // relative rank
        int stop = sq - 8;
        uint64_t path = PassedPawnMasks[BLACK][sq] & FileMasks[sq % 8];
        egScore -= P.passedOwnKingDist[r] * dist(bk, stop) + P.passedEnemyKingDist[r] * dist(wk, stop);
        if (occupancy & (1ULL << stop)) egScore -= P.passedBlocked[r];
        else if (!(path & (occupancy | whiteAttacks))) egScore -= P.passedFreePath[r];
        if (!whiteHasPieces && !(path & occupancy)) {
            int promo = sq % 8;
            int pawnDist = (r == 1) ? 5 : 7 - r;
            int kingDist = dist(wk, promo) - (board.sideToMove == WHITE ? 1 : 0);
            if (kingDist > pawnDist) egScore -= P.unstoppablePasser;
        }
    }

    return (egScore * phase) / 256;
}

// Endgames that are much more drawish than the material suggests. The endgame
// part of the score is scaled to scale/64; the middlegame part is left alone.
int EngineEval::scaleEndgame(const Board& board, int phase, int score) {
    if (score == 0) return 0;
    int scale = 64;

    uint64_t wB = board.getPieceBB(W_BISHOP), bB = board.getPieceBB(B_BISHOP);
    bool otherPieces = (board.getPieceBB(W_KNIGHT) | board.getPieceBB(B_KNIGHT) | board.getPieceBB(W_ROOK) |
                        board.getPieceBB(B_ROOK) | board.getPieceBB(W_QUEEN) | board.getPieceBB(B_QUEEN)) != 0;
    if (__builtin_popcountll(wB) == 1 && __builtin_popcountll(bB) == 1
        && isWhiteSquare(getLS1B(wB)) != isWhiteSquare(getLS1B(bB))) {
        scale = otherPieces ? P.scaleOppositeBishopsPieces : P.scaleOppositeBishops;
    }

    Color strong = score > 0 ? WHITE : BLACK;
    auto pieceValue = [&](Color c) {
        bool w = (c == WHITE);
        return VAL_KNIGHT_MG * (int) __builtin_popcountll(board.getPieceBB(w ? W_KNIGHT : B_KNIGHT))
             + VAL_BISHOP_MG * (int) __builtin_popcountll(board.getPieceBB(w ? W_BISHOP : B_BISHOP))
             + VAL_ROOK_MG   * (int) __builtin_popcountll(board.getPieceBB(w ? W_ROOK : B_ROOK))
             + VAL_QUEEN_MG  * (int) __builtin_popcountll(board.getPieceBB(w ? W_QUEEN : B_QUEEN));
    };
    if (!board.getPieceBB(strong == WHITE ? W_PAWN : B_PAWN)
        && pieceValue(strong) - pieceValue(strong == WHITE ? BLACK : WHITE) <= VAL_BISHOP_MG) {
        scale = std::min(scale, P.scaleNoPawns);
    }

    if (scale == 64) return score;
    return (int) ((int64_t) score * (64 * (256 - phase) + scale * phase) / (64 * 256));
}

int EngineEval::checkTrappedPatterns(const Board& board, int phase, Color color) {
    int penalty = 0;

    if (color == WHITE) {
        if (board.getPieceType(a7) == W_BISHOP && board.getPieceType(b6) == B_PAWN) penalty -= P.trappedBishop7th;
        if (board.getPieceType(h7) == W_BISHOP && board.getPieceType(g6) == B_PAWN) penalty -= P.trappedBishop7th;
        
        if (board.getPieceType(a6) == W_BISHOP && board.getPieceType(b7) == B_PAWN) penalty -= P.trappedBishop6th;
        if (board.getPieceType(h6) == W_BISHOP && board.getPieceType(g7) == B_PAWN) penalty -= P.trappedBishop6th;

        if (board.getPieceType(a8) == W_KNIGHT) penalty -= P.knightInCorner;
        if (board.getPieceType(h8) == W_KNIGHT) penalty -= P.knightInCorner;
        
        if (board.getPieceType(a7) == W_KNIGHT && (board.getPieceType(b6) == B_PAWN || board.getPieceType(c6) == B_PAWN)) penalty -= P.trappedKnight7th;

    } else {
        if (board.getPieceType(a2) == B_BISHOP && board.getPieceType(b3) == W_PAWN) penalty -= P.trappedBishop7th;
        if (board.getPieceType(h2) == B_BISHOP && board.getPieceType(g3) == W_PAWN) penalty -= P.trappedBishop7th;
        
        if (board.getPieceType(a3) == B_BISHOP && board.getPieceType(b2) == W_PAWN) penalty -= P.trappedBishop6th;
        if (board.getPieceType(h3) == B_BISHOP && board.getPieceType(g2) == W_PAWN) penalty -= P.trappedBishop6th;
        if (board.getPieceType(a1) == B_KNIGHT) penalty -= P.knightInCorner;
        if (board.getPieceType(h1) == B_KNIGHT) penalty -= P.knightInCorner;
        
        if (board.getPieceType(a2) == B_KNIGHT && (board.getPieceType(b3) == W_PAWN || board.getPieceType(c3) == W_PAWN)) penalty -= P.trappedKnight7th;
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
            mgPenalty += P.pinnedPiece.mg;
            egPenalty += P.pinnedPiece.eg;
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
            mgPenalty -= P.pinnedPiece.mg;
            egPenalty -= P.pinnedPiece.eg;
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
            mgPenalty += P.pinnedPiece.mg;
            egPenalty += P.pinnedPiece.eg;
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
            mgPenalty -= P.pinnedPiece.mg;
            egPenalty -= P.pinnedPiece.eg;
        }        
    }
    
    
    return (((256 - phase) * mgPenalty) + (phase * egPenalty)) / 256;
}

int EngineEval::kingSafetyEval(int phase, int dangerToBlack, int dangerToWhite) {
    int blackDanger = std::min(dangerToBlack,99);
    int whiteDanger = std::min(dangerToWhite,99);
    int overallScore = P.kingSafetyTable[blackDanger] - P.kingSafetyTable[whiteDanger];

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

int EngineEval::threatEval(const Board& board, int phase, uint64_t whitePawnAttacks, uint64_t blackPawnAttacks, uint64_t whiteMinorAttacks, uint64_t blackMinorAttacks){
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

    mgScore += (wPawnThreatsQ * P.pawnThreatQueen.mg) + (wPawnThreatsR * P.pawnThreatRook.mg) + (wPawnThreatsM * P.pawnThreatMinor.mg);
    egScore += (wPawnThreatsQ * P.pawnThreatQueen.eg) + (wPawnThreatsR * P.pawnThreatRook.eg) + (wPawnThreatsM * P.pawnThreatMinor.eg);

    int bPawnThreatsQ = (int) __builtin_popcountll(blackPawnAttacks & wQueen);
    int bPawnThreatsR = (int) __builtin_popcountll(blackPawnAttacks & wRooks);
    int bPawnThreatsM = (int) __builtin_popcountll(blackPawnAttacks & wMinors);

    mgScore -= (bPawnThreatsQ * P.pawnThreatQueen.mg) + (bPawnThreatsR * P.pawnThreatRook.mg) + (bPawnThreatsM * P.pawnThreatMinor.mg);
    egScore -= (bPawnThreatsQ * P.pawnThreatQueen.eg) + (bPawnThreatsR * P.pawnThreatRook.eg) + (bPawnThreatsM * P.pawnThreatMinor.eg);

    int wMinorThreatsQ = (int) __builtin_popcountll(whiteMinorAttacks & bQueen);
    int wMinorThreatsR = (int) __builtin_popcountll(whiteMinorAttacks & bRooks);
    mgScore += (wMinorThreatsQ * P.minorThreatQueen.mg) + (wMinorThreatsR * P.minorThreatRook.mg);
    egScore += (wMinorThreatsQ * P.minorThreatQueen.eg) + (wMinorThreatsR * P.minorThreatRook.eg);

    int bMinorThreatsQ = (int) __builtin_popcountll(blackMinorAttacks & wQueen);
    int bMinorThreatsR = (int) __builtin_popcountll(blackMinorAttacks & wRooks);
    mgScore -= (bMinorThreatsQ * P.minorThreatQueen.mg) + (bMinorThreatsR * P.minorThreatRook.mg);
    egScore -= (bMinorThreatsQ * P.minorThreatQueen.eg) + (bMinorThreatsR * P.minorThreatRook.eg);

    return (((256 - phase) * mgScore) + (phase * egScore)) / 256;
}

// Knights and bishops on squares the enemy pawns can never attack, defended by an
// own pawn, on relative ranks 4-6. Returns White minus Black.
int EngineEval::outpostEval(const Board& board, int phase, uint64_t whitePawnAttacks, uint64_t blackPawnAttacks){
    const uint64_t whiteZone = 0x0000FFFFFF000000ULL;   // ranks 4-6
    const uint64_t blackZone = 0x000000FFFFFF0000ULL;   // ranks 5-3
    uint64_t whitePawns = board.getPieceBB(W_PAWN);
    uint64_t blackPawns = board.getPieceBB(B_PAWN);
    int mgScore = 0;
    int egScore = 0;

    // Pieces of one colour standing on a safe, pawn-defended square in its zone.
    auto count = [](uint64_t pieces, uint64_t zone, uint64_t ownPawnAttacks, uint64_t enemyPawns, int side) {
        int n = 0;
        uint64_t candidates = pieces & zone & ownPawnAttacks;
        while (candidates) {
            int sq = getLS1B(candidates);
            uint64_t adjacentAhead = PassedPawnMasks[side][sq] & ~FileMasks[sq % 8];
            if (!(adjacentAhead & enemyPawns)) n++;
            popBit(candidates, sq);
        }
        return n;
    };

    int knights = count(board.getPieceBB(W_KNIGHT), whiteZone, whitePawnAttacks, blackPawns, 0)
                - count(board.getPieceBB(B_KNIGHT), blackZone, blackPawnAttacks, whitePawns, 1);
    int bishops = count(board.getPieceBB(W_BISHOP), whiteZone, whitePawnAttacks, blackPawns, 0)
                - count(board.getPieceBB(B_BISHOP), blackZone, blackPawnAttacks, whitePawns, 1);
    mgScore += knights * P.knightOutpost.mg + bishops * P.bishopOutpost.mg;
    egScore += knights * P.knightOutpost.eg + bishops * P.bishopOutpost.eg;

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