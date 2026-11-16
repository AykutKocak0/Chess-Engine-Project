#include <stdint.h>
#include <cstring>
#include "Types.h"
#include <string>
#include "Board.h"
#include "Search.h"
#include "GenerateMoves.h"
#include <limits>
#include <algorithm>
#include <iostream>
#include "TransPositionTable.h"
#include <cassert>
#include <math.h>
#include "Constants.h"
#include "TimeManagement.h"
#include <mutex>
#include <sstream>

using namespace Constants;

static std::mutex uciOutputMutex;

void uciPrint(const std::string& line) {
    std::lock_guard<std::mutex> lock(uciOutputMutex);
    std::cout << line << std::endl;
}

void Search::startAsync(const Board& board, const SearchLimits& limits) {
    stopAndWait();
    // Reset before the thread starts, so a "stop" that arrives right after "go" is not lost.
    stopped = false;
    infinite = limits.infinite;
    // The thread gets its own (non-const) copy of the board to make moves on.
    worker = std::thread([this, boardCopy = Board(board), limits]() mutable {
        startSearch(boardCopy, limits);
    });
}

void Search::stopAndWait() {
    stopped = true;
    if (worker.joinable()) worker.join();
}

void Search::waitForFinish() {
    if (worker.joinable()) worker.join();
}

SearchResult Search::searchSync(Board& board, const SearchLimits& limits) {
    stopAndWait();
    stopped = false;
    infinite = false;
    silent = true;
    startSearch(board, limits);
    silent = false;

    SearchResult result;
    result.hasMove = hasRootMove;
    result.move = bestMoveFound;
    result.score = bestScoreFound;
    result.isMate = std::abs(bestScoreFound) > MATE_BOUND;
    return result;
}

void Search::startSearch(Board& board, const SearchLimits& limits) {
    const int maxDepth = limits.depth;
    nodes = 0;
    nodeLimit = limits.nodes;
    hasRootMove = false;
    bestScoreFound = 0;
    startTime = std::chrono::high_resolution_clock::now();
    int phase = board.getPhase();
    // "go infinite" ignores any clock values; "movetime" wins over the clock.
    bool isTimeLeft = !limits.infinite && (limits.moveTime > 0 || limits.timeLeft > 0);
    if (isTimeLeft) {
        if (limits.moveTime > 0) timeManager.startFixed(limits.moveTime);
        else timeManager.startTimer(limits.timeLeft, limits.increment, limits.movesToGo, phase);
    }
    // UCI: in infinite mode bestmove may only be sent after "stop".
    auto printBestMove = [this](const std::string& move) {
        if (silent) return;
        while (infinite && !stopped) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        uciPrint("bestmove " + move);
    };

    memset(historyTable, 0, sizeof(historyTable));
    for (int i = 0; i < MAX_PLY; i++) {
        killerMoves[i][0] = Move();
        killerMoves[i][1] = Move();
        stackMoves[i] = Move();
        stackPieces[i] = -1;
    }
    for (int p = 0; p < 12; p++) {
        for (int sq = 0; sq < 64; sq++) counterMoves[p][sq] = Move();
    }
    std::fill(contHist.begin(), contHist.end(), 0);

    timed = isTimeLeft;
    TT.newSearch();
    betaCutoffs = 0;
    firstMoveCutoffs = 0;

    Move pseudoMoves[256];
    int pseudoCount = gen.generateMoves(board, pseudoMoves);

    // Keep only legal root moves so mate/stalemate is detected up front.
    Move rootMoveList[256];
    int moveCount = 0;
    for (int i = 0; i < pseudoCount; i++) {
        if (board.makeMove(pseudoMoves[i])) {
            board.unmakeMove(pseudoMoves[i]);
            rootMoveList[moveCount++] = pseudoMoves[i];
        }
    }
    if (moveCount == 0) {
        printBestMove("0000");
        return;
    }
    for (int i = 0; i < moveCount; i++) {
        rootMoveList[i].score = scoreMove(board, rootMoveList[i], 0);
    }
    // Fallback in case the search is stopped before depth 1 completes.
    hasRootMove = true;
    bestMoveFound = rootMoveList[0];
    int previousBestValue = 0;
    // Principal variation of the best root move in the current iteration.
    Move rootPV[65];
    int rootPVLength = 0;
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
                    rootMoveList[i].score = SCORE_HASH;
                    break; 
                }
            }
        }
        int movesSearched = 0;
        for (int i = 0; i < moveCount; i++) {
            pickBestMove(rootMoveList, moveCount, i);
            stackMoves[0] = rootMoveList[i];
            stackPieces[0] = board.getPieceType(rootMoveList[i].from);
            if (!board.makeMove(rootMoveList[i])) continue;

            if (movesSearched == 0) {
                score = -alphaBeta(board, -beta, -alpha, currentDepth - 1, 1, true);
                if ((score <= alpha || score >= beta) && abs(score) < MATE_SCORE) {
                
                alpha = -1000000;
                beta  =  1000000;
                
                score = -alphaBeta(board, -beta, -alpha, currentDepth - 1, 1, true);
            }
            } else {
                bool isKiller = (rootMoveList[i] == killerMoves[0][0] || rootMoveList[i] == killerMoves[0][1]);
                int reduction = 0;
                if (currentDepth >= 3 && i >= 4 && !rootMoveList[i].isCapture() && !rootMoveList[i].isPromotion() && !wasChecked && !isKiller) {
                    reduction = reductionTable[std::min(currentDepth, 63)][std::min(i, 63)];
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

            if (score > -MATE_SCORE && score < MATE_SCORE) {
                rootMoveList[i].score = score; 
            }

            if (score > bestValue) {
                bestValue = score;
                currentBestMove = rootMoveList[i];
                // The child at ply 1 left its line in pvTable[1]; prepend the root move.
                rootPV[0] = rootMoveList[i];
                rootPVLength = 1;
                for (int p = 1; p < pvLength[1]; p++) rootPV[rootPVLength++] = pvTable[1][p];
            }
            alpha = std::max(alpha, score);
        }

        if (!stopped) {
            bestMoveFound = currentBestMove;
            previousBestValue = bestValue;
            bestScoreFound = bestValue;

            auto now = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed = now - startTime;
            uint64_t nps = (elapsed.count() > 0.001) ? (uint64_t)(nodes / elapsed.count()) : 0;

            std::ostringstream info;
            info << "info depth " << currentDepth
                 << " score " << scoreToUci(bestValue)
                 << " nodes " << nodes
                 << " nps " << nps
                 << " time " << (int)(elapsed.count() * 1000)
                 << " pv";
            for (int p = 0; p < rootPVLength; p++) info << " " << moveToString(rootPV[p]);
            if (!silent) uciPrint(info.str());
            if (bestValue > MATE_BOUND) { 

                int mateDistance = MATE_SCORE - bestValue; 
                if (mateDistance <= currentDepth) {
                    break;
                }
            }
        } else {
            break; 
        }
    }

    // Move-ordering quality: share of beta cutoffs that happened on the first move tried.
    double firstMoveRate = betaCutoffs ? (100.0 * firstMoveCutoffs / betaCutoffs) : 0.0;
    std::ostringstream stats;
    stats << "info string cutoffs " << betaCutoffs
          << " first-move " << firstMoveCutoffs
          << " rate " << firstMoveRate << "%";
    if (!silent) uciPrint(stats.str());

    printBestMove(moveToString(bestMoveFound));
}

int Search::alphaBeta(Board& board, int alpha, int beta, int depthLeft, int ply, bool allowNull) {
    nodes++;
    // Empty PV until a move raises alpha; must happen before any early return.
    pvLength[ply] = ply;
    if (timed && (nodes & 2047) == 0) {
        if (timeManager.isTimeUp()) {
            stopped = true;
        }
    }
    if (nodeLimit && nodes >= nodeLimit) stopped = true;
    if (ply > 0 && (board.isRepetition() || board.getMovesWH() >=100)) {

        return 0;
    }
    if (ply >= MAX_PLY) return board.evaluate();
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
        if (ttScore > MATE_BOUND) ttScore -= ply;
        else if (ttScore < -MATE_BOUND) ttScore += ply;
        if (entry->depth >= depthLeft) {
                if (entry->flag == HASH_EXACT) return ttScore;
                if (entry->flag == HASH_ALPHA && ttScore <= alpha) return ttScore;
                if (entry->flag == HASH_BETA && ttScore >= beta) return ttScore;
        }
    }
    bool wasChecked = board.isInCheck();

    if (!wasChecked && depthLeft <= 2) {
        int staticEval = board.evaluate();
        
        int margin = 300 + 150 * depthLeft * depthLeft; 
        
        if (staticEval < alpha - margin) {
            int value = quiescence(board, alpha - 1, alpha, ply, 0);
            
            if (value < alpha && std::abs(value) < MATE_BOUND) {
                return value; 
            }
        }
    }

    if(!wasChecked && !board.isZugzwang() && depthLeft >= 3 && allowNull){
        int nullReduction = 3;
        stackPieces[ply] = -1;
        board.makeNullMove();
        int nullScore = -alphaBeta(board, -beta, -beta+1, depthLeft-1-nullReduction,ply+1, false);
        board.unmakeNullMove();

        if (stopped) return 0;
        if(nullScore >= beta){
            return nullScore;
        }
    }
    // Internal iterative reduction: without a hash move the ordering is weak,
    // so search this node one ply shallower and let the TT fill in for next time.
    if (!hasHashMove && depthLeft >= 4) depthLeft--;

    Move moveList[256];
    bool moveMade = false;
    int moveCount = gen.generateMoves(board, moveList);
    int bestValue = -1000000;
    for (int i = 0; i < moveCount; i++) {
        if(hasHashMove && moveList[i] == hashMove){
            moveList[i].score = SCORE_HASH;
        }
        else{ moveList[i].score = scoreMove(board, moveList[i], ply); }
    }
    int score;
    Move quietsTried[64];
    int quietsTriedCount = 0;
    bool isEndGame = board.isEndGame();
    int movesSearched = 0;
    for (int i = 0; i < moveCount; ++i) { 
        pickBestMove(moveList, moveCount, i);
        stackMoves[ply] = moveList[i];
        stackPieces[ply] = board.getPieceType(moveList[i].from);
        if(!board.makeMove(moveList[i])) continue;
        if (movesSearched == 0) {
            score = -alphaBeta(board, -beta, -alpha, depthLeft - 1, ply + 1, true);
        } else {
            bool isKiller = (moveList[i] == killerMoves[ply][0] || moveList[i] == killerMoves[ply][1]);
            int reduction = 0;
            if (depthLeft >= 3 && i >= 4 && !moveList[i].isCapture() && !moveList[i].isPromotion() && !wasChecked && !isKiller) {
                reduction = reductionTable[std::min(depthLeft, 63)][std::min(i, 63)];
                reduction = std::max(0, std::min(reduction, depthLeft - 2));
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
        // An aborted search returns meaningless scores; never let them reach the TT.
        if (stopped) return 0;
        moveMade = true;
        movesSearched++;
        if (score > bestValue) {
            bestMove = moveList[i];
            bestValue = score;
        }
        bool isQuiet = !moveList[i].isCapture() && !moveList[i].isPromotion();
        if (score >= beta) {
            betaCutoffs++;
            if (movesSearched == 1) firstMoveCutoffs++;
            if (isQuiet) {
                if (moveList[i] != killerMoves[ply][0]) {
                    killerMoves[ply][1] = killerMoves[ply][0];
                    killerMoves[ply][0] = moveList[i];
                }
                int bonus = depthLeft * depthLeft;
                if (bonus > 400) bonus = 400;
                updateHistory(board, moveList[i], ply, bonus);
                // Malus: quiets that were tried first and failed to cut off.
                for (int q = 0; q < quietsTriedCount; q++) {
                    updateHistory(board, quietsTried[q], ply, -bonus);
                }
                if (ply >= 1 && stackPieces[ply - 1] >= 0) {
                    counterMoves[stackPieces[ply - 1]][stackMoves[ply - 1].to] = moveList[i];
                }
            }
            int storeScore = score;
            if (storeScore > MATE_BOUND) storeScore += ply;
            else if (storeScore < -MATE_BOUND) storeScore -= ply;
            
            TT.store(zobristKey, depthLeft, storeScore, HASH_BETA, moveList[i]);
            return score;
        }
        if (isQuiet && quietsTriedCount < 64) quietsTried[quietsTriedCount++] = moveList[i];
        if (score > alpha) {
            alpha = score;
            // New best line: this move followed by the child's PV.
            pvTable[ply][ply] = moveList[i];
            for (int p = ply + 1; p < pvLength[ply + 1]; p++) pvTable[ply][p] = pvTable[ply + 1][p];
            pvLength[ply] = pvLength[ply + 1];
        }
    }
    if(!moveMade){
        if (board.isInCheck()) return -MATE_SCORE + ply; 
        
        return 0;
    }
    HashFlag flag = HASH_ALPHA; 
    if (bestValue >= beta) flag = HASH_BETA;
    else if (bestValue > originalAlpha) flag = HASH_EXACT;

    int storeScore = bestValue;
    if (storeScore > MATE_BOUND) storeScore += ply;
    else if (storeScore < -MATE_BOUND) storeScore -= ply;

    TT.store(zobristKey, depthLeft, storeScore, flag, bestMove);

    return bestValue;
}

int Search::quiescence(Board& board, int alpha, int beta, int ply, int qsDepth) {
    nodes++;
    if (ply >= MAX_PLY) return board.evaluate();
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
            if (moveList[i].isCapture() || moveList[i].isCapturePromotion()) {
                moveList[i].score = scoreCapture(board, moveList[i]);
            } else {
                // Quiet checks go after every SEE-passing capture (those score >= 100).
                moveList[i].score = 0;
            }
        }
    }
    for (int i = 0; i < moveCount; i++) {
        pickBestMove(moveList, moveCount, i);
        // SEE needs a piece on the target square: skip en passant and non-captures.
        bool seeApplies = moveList[i].flag == MoveFlag::CAPTURE || moveList[i].isCapturePromotion();

        if(seeApplies && !inCheck){
            bool SEE = board.SEECalculation(board,moveList[i].to,board.sideToMove);
            if(!SEE) continue;
        }
        stackMoves[ply] = moveList[i];
        stackPieces[ply] = board.getPieceType(moveList[i].from);
        if (!board.makeMove(moveList[i])) continue;
        legalMovesSearched++;

        int score = -quiescence(board, -beta, -alpha, ply + 1, qsDepth + 1);
        
        board.unmakeMove(moveList[i]);

        if (score >= beta) return score;
        if (score > alpha) alpha = score;
    }
    
    if (inCheck && legalMovesSearched == 0) {
        return -MATE_SCORE + ply; 
    }
    return alpha;
}

int Search::scoreCapture(Board& board, Move& move, bool* losing) {
    int pieceMoved = board.getPieceType(move.from);
    int pieceTaken = board.getPieceType(move.to);

    if (move.flag == MoveFlag::EN_PASSANT) {
        pieceTaken = (board.sideToMove == WHITE) ? B_PAWN : W_PAWN;
    }

    if (losing) *losing = false;
    int score = (PIECE_VALUES[pieceTaken] * 10) - PIECE_VALUES[pieceMoved];
    // En passant is pawn-for-pawn at worst and its target square is empty, so SEE does not apply.
    if (move.flag == MoveFlag::EN_PASSANT) return score;
    // A capture can only lose material when the attacker is worth more than the victim.
    if (PIECE_VALUES[pieceMoved] > PIECE_VALUES[pieceTaken]) {
        if (!board.SEECalculation(board, move.to, board.sideToMove)) {
            score -= 5000;
            if (losing) *losing = true;
        }
    }

    return score;
}

// Ordering bands, highest first:
//   hash move > good captures / queen promotions > killers > quiets (history)
//   > losing captures > underpromotions
int Search::scoreMove(Board& board, Move& move, int ply) {
    bool queenPromo = (move.flag == MoveFlag::PROMOTION_QUEEN || move.flag == MoveFlag::CPROMOTION_QUEEN);
    if (move.isPromotion() && !queenPromo) {
        return SCORE_UNDERPROMO;
    }

    if (move.isCapture() || move.isCapturePromotion()) {
        bool losing = false;
        int captureScore = scoreCapture(board, move, &losing);
        if (losing) return SCORE_BAD_CAPTURE + captureScore;
        return SCORE_GOOD_CAPTURE + captureScore + (queenPromo ? 9000 : 0);
    }

    if (queenPromo) {
        return SCORE_GOOD_CAPTURE + 9000;
    }

    if (killerMoves[ply][0] == move) return SCORE_KILLER_1;
    if (killerMoves[ply][1] == move) return SCORE_KILLER_2;

    // Quiet move: butterfly history plus continuation history of the last two plies.
    int piece = board.getPieceType(move.from);
    int score = historyTable[board.sideToMove][move.from][move.to];
    if (ply >= 1 && stackPieces[ply - 1] >= 0) {
        int prevPiece = stackPieces[ply - 1];
        int prevTo = stackMoves[ply - 1].to;
        if (counterMoves[prevPiece][prevTo] == move) return SCORE_COUNTER;
        score += contHistEntry(prevPiece, prevTo, piece, move.to);
    }
    if (ply >= 2 && stackPieces[ply - 2] >= 0) {
        score += contHistEntry(stackPieces[ply - 2], stackMoves[ply - 2].to, piece, move.to);
    }
    return score;
}

// Updates butterfly and continuation history for a quiet move at this ply.
// Gravity keeps each table within +/-HISTORY_MAX for both bonus and malus.
// Must be called with the move unmade (board at the node's own position).
void Search::updateHistory(const Board& board, const Move& move, int ply, int bonus) {
    auto apply = [bonus](int& entry) {
        entry += bonus - entry * std::abs(bonus) / HISTORY_MAX;
    };
    apply(historyTable[board.sideToMove][move.from][move.to]);

    int piece = board.getPieceType(move.from);
    for (int back = 1; back <= 2; back++) {
        int p = ply - back;
        if (p >= 0 && stackPieces[p] >= 0) {
            apply(contHistEntry(stackPieces[p], stackMoves[p].to, piece, move.to));
        }
    }
}

void Search::pickBestMove(Move* moveList, int moveCount, int currentIndex) {
    int bestScore = moveList[currentIndex].score;
    int bestIndex = currentIndex;

    for (int i = currentIndex + 1; i < moveCount; i++) {
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

// UCI wants mate distance in moves, not plies: positive = we mate, negative = we get mated.
std::string Search::scoreToUci(int score) {
    if (score > MATE_BOUND) {
        int plies = MATE_SCORE - score;           // 89999 -> mate in 1 ply
        return "mate " + std::to_string((plies + 1) / 2);
    }
    if (score < -MATE_BOUND) {
        int plies = MATE_SCORE + score;           // -89998 -> mated in 2 plies
        return "mate -" + std::to_string(plies / 2);
    }
    return "cp " + std::to_string(score);
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
