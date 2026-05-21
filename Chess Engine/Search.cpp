#include <stdint.h>
#include "Types.h"
#include <string>
#include "Board.h"
#include "Search.h"
#include "GenerateMoves.h"
#include <limits>
#include <iostream>
#include "TransPositionTable.h"
#include <cassert>
#include <math.h>
#include "Constants.h"
#include "TimeManagement.h"

using namespace Constants;


Move bestMoveFound;

void Search::startSearch(Board& board, int maxDepth, int timeLeft, int increment, int movesToGo) {
    nodes = 0; 
    startTime = std::chrono::high_resolution_clock::now();
    stopped = false;
    int alpha = -1000000;
    int beta = 1000000;
    int phase = board.getPhase();
    bool isTimeLeft = (timeLeft > 0);
    if(isTimeLeft) timeManager.startTimer(timeLeft,increment,movesToGo,phase);

    memset(historyTable, 0, sizeof(historyTable));
    for (int i = 0; i < MAX_PLY; i++) {
        killerMoves[i][0] = Move();
        killerMoves[i][1] = Move();
    }

    Move rootMoveList[256];
    int moveCount = gen.generateMoves(board, rootMoveList);
    
    for (int i = 0; i < moveCount; i++) { 
        rootMoveList[i].score = scoreMove(board, rootMoveList[i], 0); 
        
    }
    if(moveCount== 0){
        return;
    }
    int previousBestValue = 0; 
    for (int currentDepth = 1; currentDepth <= maxDepth; currentDepth++) {
        if(isTimeLeft){
            if(timeManager.isTimeUp()) break;
            int allocatedTime = timeManager.getAllocatedTimeMs();
            int elapsedTime = timeManager.getElapsedTimeMs();
           // if((allocatedTime * 6) / 10 <= elapsedTime) break;
        }
        int alpha = -1000000;
        int beta  =  1000000;
        
        if (currentDepth >= 3) {
            alpha = previousBestValue - 50; 
            beta  = previousBestValue + 50;
        }

        int bestValue = -1000000;
        int score;
        Move currentBestMove; 
        bool hasRootHash = (currentDepth > 1); 
        bool isEndGame = board.isEndGame();
        bool wasChecked = board.isInCheck();
        if (hasRootHash) {
            for (int i = 0; i < moveCount; i++) {
                if (rootMoveList[i] == bestMoveFound) {
                    rootMoveList[i].score = 2000000; 
                    break; 
                }
            }
        }
        int movesSearched = 0;
        for (int i = 0; i < moveCount; i++) {
            pickBestMove(rootMoveList, moveCount, i);
            if (!board.makeMove(rootMoveList[i])) continue;

            if (movesSearched == 0) {
                score = -alphaBeta(board, -beta, -alpha, currentDepth - 1, 1, true);
                if ((score <= alpha || score >= beta) && abs(score) < 90000) {
                
                alpha = -1000000;
                beta  =  1000000;
                
                score = -alphaBeta(board, -beta, -alpha, currentDepth - 1, 1, true);
            }
            } else {
                bool isKiller = (rootMoveList[i] == killerMoves[0][0] || rootMoveList[i] == killerMoves[0][1]);
                int reduction = 0;
                if (currentDepth >= 3 && i >= 4 && !rootMoveList[i].isCapture() && !rootMoveList[i].isPromotion() && !wasChecked && !isKiller) {
                    reduction = reductionTable[currentDepth][i];
                    reduction = std::max(0, std::min(reduction, currentDepth - 2));
                    if(isEndGame){
                        reduction = (reduction == 0) ? reduction : reduction-1;
                    }
                }
                
                score = -alphaBeta(board, -alpha - 1, -alpha, currentDepth - 1 - reduction, 1, true);

                if (score > alpha) {
                    if (reduction > 0) {
                        score = -alphaBeta(board, -alpha - 1, -alpha, currentDepth - 1, 1, true);
                    }
                    if (score > alpha && score < beta) {
                        score = -alphaBeta(board, -beta, -alpha, currentDepth - 1, 1, true);
                    }
                }
            }
            board.unmakeMove(rootMoveList[i]);
            movesSearched++;

            if (stopped) break;

            if (score > -90000 && score < 90000) {
                rootMoveList[i].score = score; 
            }

            if (score > bestValue) {
                bestValue = score;
                currentBestMove = rootMoveList[i];
            }
            alpha = std::max(alpha, score);
        }

        if (!stopped) {
            bestMoveFound = currentBestMove; 
            previousBestValue = bestValue; 

            auto now = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed = now - startTime;
            uint64_t nps = (elapsed.count() > 0.001) ? (uint64_t)(nodes / elapsed.count()) : 0;

            std::cout << "info depth " << currentDepth 
                    << " score cp " << bestValue 
                    << " nodes " << nodes 
                    << " nps " << nps 
                    << " time " << (int)(elapsed.count() * 1000) 
                    << " pv " << moveToString(bestMoveFound) << std::endl;
            if (bestValue > 80000) { 

                int mateDistance = 90000 - bestValue; 
                if (mateDistance <= currentDepth) {
                    break;
                }
            }
        } else {
            break; 
        }
    }

    std::cout << "bestmove " << moveToString(bestMoveFound) << std::endl;
}

int Search::alphaBeta(Board& board, int alpha, int beta, int depthLeft, int ply, bool allowNull) {
    nodes++;
    if ((nodes & 2047) == 0) {
        if (timeManager.isTimeUp()) {
            stopped = true;
    }
}
    if (ply > 0 && (board.isRepetition() || board.gameState.fiftyMoveClock >=100)) {
        
        return 0;
    }
    int originalAlpha = alpha;
    Move hashMove;
    bool hasHashMove = false;
    uint64_t zobristKey = board.getZobristKey();

    Move bestMove;
    if(ply < MAX_PLY){
        if(board.isInCheck()) depthLeft++;
    }
    if(stopped) return 0;
    if (depthLeft <= 0){
        return quiescence(board, alpha, beta, ply, 0);
    } 
    TTEntry* entry = TT.probe(zobristKey);
    if (entry != nullptr) {
        hashMove = entry->bestMove;
        hasHashMove = true;
        int ttScore = entry->score;
        if (ttScore > 80000) ttScore -= ply;
        else if (ttScore < -80000) ttScore += ply;
        if (entry->depth >= depthLeft) {
                if (entry->flag == HASH_EXACT) return ttScore;
                if (entry->flag == HASH_ALPHA && ttScore <= alpha) return ttScore;
                if (entry->flag == HASH_BETA && ttScore >= beta) return ttScore;
        }
    }
    bool wasChecked = board.isInCheck();
    if(!wasChecked && !board.isZugzwang() && depthLeft >= 3 && allowNull){
        int nullReduction = 3;
        board.makeNullMove();
        int nullScore = -alphaBeta(board, -beta, -beta+1, depthLeft-1-nullReduction,ply+1, false);
        board.unmakeNullMove();

        if(nullScore >= beta){
            return nullScore;
        }
    }
    Move moveList[256];
    bool moveMade = false;
    int moveCount = gen.generateMoves(board, moveList);
    int bestValue = -1000000;
    for (int i = 0; i < moveCount; i++) {
        if(hasHashMove && moveList[i].from == hashMove.from && moveList[i].to == hashMove.to){
            moveList[i].score = 200000;
        }
        else{ moveList[i].score = scoreMove(board, moveList[i], ply); }
    }
    int score;
    bool isEndGame = board.isEndGame();
    int movesSearched = 0;
    for (int i = 0; i < moveCount; ++i) { 
        pickBestMove(moveList, moveCount, i);
        if(!board.makeMove(moveList[i])) continue;
        if (movesSearched == 0) {   
            score = -alphaBeta(board, -beta, -alpha, depthLeft - 1, ply + 1, true);
        } else {
            bool isKiller = (moveList[i] == killerMoves[ply][0] || moveList[i] == killerMoves[ply][1]);
            int reduction = 0;
            if (depthLeft >= 3 && i >= 4 && !moveList[i].isCapture() && !moveList[i].isPromotion() && !wasChecked && !isKiller) {
                reduction = reductionTable[depthLeft][i];
                reduction = std::min(reduction, depthLeft - 2);        
            }
            score = -alphaBeta(board, -alpha - 1, -alpha, depthLeft - 1 - reduction, ply + 1, true);

            if (score > alpha) {
                if (reduction > 0) {
                    score = -alphaBeta(board, -alpha - 1, -alpha, depthLeft - 1, ply + 1, true);
                }
                if (score > alpha && score < beta) {
                    score = -alphaBeta(board, -beta, -alpha, depthLeft - 1, ply + 1, true);
                }
            }
        }

        board.unmakeMove(moveList[i]);
        moveMade = true;
        movesSearched++;
        if (score > bestValue) {
            bestMove = moveList[i];
            bestValue = score;
        }
        if (score >= beta) {
            if (!moveList[i].isCapture()  && !moveList[i].isPromotion()) {
            if (moveList[i] != killerMoves[ply][0]) {
                killerMoves[ply][1] = killerMoves[ply][0];
                killerMoves[ply][0] = moveList[i];
            }
            int bonus = depthLeft * depthLeft;
            if (bonus > 400) bonus = 400; 
            int currentHistory = historyTable[board.sideToMove][moveList[i].from][moveList[i].to];
            historyTable[board.sideToMove][moveList[i].from][moveList[i].to] += bonus - (currentHistory * bonus / 10000); 
        }
            int storeScore = score;
            if (storeScore > 80000) storeScore += ply;
            else if (storeScore < -80000) storeScore -= ply;
            
            TT.store(zobristKey, depthLeft, storeScore, HASH_BETA, moveList[i]);
            return score;
        }
        if (score > alpha) {
            alpha = score; 
        }
    }
    if(!moveMade){
        if (board.isInCheck()) return -90000 + ply; 
        
        return 0;
    }
    HashFlag flag = HASH_ALPHA; 
    if (bestValue >= beta) flag = HASH_BETA;
    else if (bestValue > originalAlpha) flag = HASH_EXACT;

    int storeScore = bestValue;
    if (storeScore > 80000) storeScore += ply;
    else if (storeScore < -80000) storeScore -= ply;

    TT.store(zobristKey, depthLeft, storeScore, flag, bestMove);

    return bestValue;
}

int Search::quiescence(Board& board, int alpha, int beta, int ply, int qsDepth) {
    nodes++;
    bool inCheck = board.isInCheck();
    if (!inCheck) {
        int standPat = board.evaluate();
        if (standPat >= beta) return standPat;
        if (standPat > alpha) alpha = standPat;
    }

    Move moveList[256];
    int moveCount = 0;

    if (inCheck) {
        moveCount = gen.generateMoves(board, moveList);
    } else {
        moveCount = gen.generateCaptureMoves(board, moveList, board.sideToMove);
        
        if (qsDepth < 2) { 
            moveCount = gen.generateQuietCheckMoves(board, moveList, board.sideToMove, moveCount);
        }
    }

    int legalMovesSearched = 0;

    for (int i = 0; i < moveCount; i++) {
        if (inCheck) {
            moveList[i].score = scoreMove(board, moveList[i], ply);
        } else {
            if (moveList[i].flag != MoveFlag::QUIET) { 
                moveList[i].score = scoreCapture(board, moveList[i]);
            } else {
                moveList[i].score = 50000; 
            }
        }
    }
    for (int i = 0; i < moveCount; i++) {
        pickBestMove(moveList, moveCount, i);
        bool checkMove = moveList[i].flag == MoveFlag::QUIET;
        bool SEE = board.SEECalculation(board,moveList[i].to,board.sideToMove);

        if(!checkMove && !inCheck){
            if(!SEE) continue;
        }
        if (!board.makeMove(moveList[i])) continue;
        legalMovesSearched++;

        int score = -quiescence(board, -beta, -alpha, ply + 1, qsDepth + 1);
        
        board.unmakeMove(moveList[i]);

        if (score >= beta) return score;
        if (score > alpha) alpha = score;
    }
    
    if (inCheck && legalMovesSearched == 0) {
        return -90000 + ply; 
    }
    return alpha;
}

int Search::scoreCapture(Board& board, Move& move) {
    int pieceMoved = board.getPieceType(move.from);
    int pieceTaken = board.getPieceType(move.to);

    if (move.flag == MoveFlag::EN_PASSANT) {
        pieceTaken = (board.sideToMove == WHITE) ? B_PAWN : W_PAWN;
    }

    
    int score = (PIECE_VALUES[pieceTaken] * 10) - PIECE_VALUES[pieceMoved];
    bool SEE = board.SEECalculation(board, move.to, board.sideToMove);
    if (PIECE_VALUES[pieceMoved] > PIECE_VALUES[pieceTaken]) {
        if (!SEE) {
            score -= 5000; 
        }
    }

    return score;
}

int Search::scoreMove(Board& board, Move& move, int ply) {
    

    if (move.isCapture()) {
        return 10000 + scoreCapture(board, move);
    }
    
    
    if (move.isPromotion()) {
        return 9000; 
    }

    if (killerMoves[ply][0] == move) return 8000;
    if (killerMoves[ply][1] == move) return 7000;

    return historyTable[board.sideToMove][move.from][move.to]; 
}

void Search::pickBestMove(Move* moveList, int moveCount, int currentIndex) {
    int bestScore = -100000;
    int bestIndex = currentIndex;

    for (int i = currentIndex; i < moveCount; i++) {
        if (moveList[i].score > bestScore) {
            bestScore = moveList[i].score;
            bestIndex = i;
        }
    }

    Move temp = moveList[currentIndex];
    moveList[currentIndex] = moveList[bestIndex];
    moveList[bestIndex] = temp;
}

std::string Search::moveToString(Move move) {
    int fileFrom = move.from % 8;
    int rankFrom = move.from / 8;
    int fileTo = move.to % 8;
    int rankTo = move.to / 8;

    std::string moveStr = "";
    moveStr += (char)('a' + fileFrom);
    moveStr += (char)('1' + rankFrom);
    moveStr += (char)('a' + fileTo);
    moveStr += (char)('1' + rankTo);

    if (move.isPromotion()) {
        char promChar = getPromotionChar(move); 
        moveStr += promChar;
    }

    return moveStr;
}

char Search::getPromotionChar(Move move) {
    switch (move.flag) {
        case MoveFlag::PROMOTION_QUEEN:
        case MoveFlag::CPROMOTION_QUEEN:  return 'q';
      
        case MoveFlag::PROMOTION_ROOK:
        case MoveFlag::CPROMOTION_ROOK:   return 'r';
        
        case MoveFlag::PROMOTION_BISHOP:
        case MoveFlag::CPROMOTION_BISHOP: return 'b';
        
        case MoveFlag::PROMOTION_KNIGHT:
        case MoveFlag::CPROMOTION_KNIGHT: return 'n';
        
        default: return '\0';
    }
}
