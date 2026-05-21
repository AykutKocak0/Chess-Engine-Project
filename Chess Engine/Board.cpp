#include <stdint.h>
#include "Types.h"
#include "Board.h"
#include <iostream>
#include <string>
#include "GenerateMoves.h"
#include "EngineEval.h"
#include "Constants.h"
#include "TransPositionTable.h"
#include "Search.h"

using namespace Constants;

void Board::clear() {
    for(int i = 0; i < 12; ++i) piecesBB[i] = 0ULL;
    for(int i = 0; i < 3; ++i) colorBB[i] = 0ULL;
    sideToMove = WHITE;
    enPassantSq = -1;
    castlingRights = 0b1111;
    castleHappened = 0b0000;
    gameState.capturedPiece = -1;
    gameState.enPassantSq = -1;
    gameState.castlingRights = 0b1111;
    gameState.fiftyMoveClock = 0;
    gameState.oldFiftyMoveClock = 0;
    historyCount = 0;

}

Board::Board(){
    castleHappened = 0b0000;
    clear();
    setupBoardArray();
    Constants::initWhole();
}

void Board::updateOccupancy(){
    colorBB[WHITE] = piecesBB[W_PAWN] | piecesBB[W_ROOK] | piecesBB[W_KNIGHT]
    | piecesBB[W_BISHOP] | piecesBB[W_QUEEN] | piecesBB[W_KING];

    colorBB[BLACK] = piecesBB[B_PAWN] | piecesBB[B_ROOK] | piecesBB[B_KNIGHT]
    | piecesBB[B_BISHOP] | piecesBB[B_QUEEN] | piecesBB[B_KING];

    colorBB[BOTH] = colorBB[WHITE] | colorBB[BLACK];
}

void Board::setupBoardArray() {
    for (int i = 0; i < 64; i++) {
        boardArray[i] = -1;
    }

    for (int p = 0; p < 12; p++) {
        uint64_t bb = piecesBB[p];
        while (bb) {
            int sq = getLS1B(bb);
            boardArray[sq] = p; 
            popBit(bb, sq);
        }
    }
}  

void Board::setupStartingBoard(){
    clear();
    piecesBB[W_PAWN] = 0x000000000000FF00;
    piecesBB[W_ROOK]   = (1ULL << 0) | (1ULL << 7); 
    piecesBB[W_KNIGHT] = (1ULL << 1) | (1ULL << 6); 
    piecesBB[W_BISHOP] = (1ULL << 2) | (1ULL << 5); 
    piecesBB[W_QUEEN]  = (1ULL << 3);              
    piecesBB[W_KING]   = (1ULL << 4);
    piecesBB[B_PAWN] = 0x00FF000000000000;
    piecesBB[B_ROOK]   = (1ULL << 63) | (1ULL << 56); 
    piecesBB[B_KNIGHT] = (1ULL << 62) | (1ULL << 57); 
    piecesBB[B_BISHOP] = (1ULL << 61) | (1ULL << 58); 
    piecesBB[B_QUEEN]  = (1ULL << 59);              
    piecesBB[B_KING]   = (1ULL << 60);
    updateOccupancy();
    setupBoardArray();
    sideToMove = WHITE;
    initZobristKey();
    initPawnKey();
    historyCount =0;
    movesWithoutHalf = 0;
    castleHappened = 0b0000;
}

uint64_t Board::getOccupancy(Color color) const {
    return colorBB[color];
}

uint64_t Board::getPieceBB(PieceType pT) const {
    return piecesBB[pT];
}

int Board::getPieceType(int sq) const {
    return boardArray[sq];
}

uint64_t Board::getZobristKey() const {
    return zobristKey;
}

uint64_t Board::getPawnKey() const {
    return pawnKey;
}

int Board::getKingSquare(Color color) const {
        if(color == WHITE) return whiteKingSq;
        return blackKingSq;
}

void printBoard(uint64_t bb) {
    for (int rank = 7; rank >= 0; rank--) {
        for (int file = 0; file < 8; file++) {
            int sq = rank * 8 + file;
            std::cout << ((bb >> sq) & 1ULL ? "1 " : ". ");
        }
        std::cout << std::endl;
    }
    std::cout << "  a b c d e f g h" << std::endl << std::endl;
}

void Board::parseFEN(const std::string& fen) {
    clear();
    for (int i = 0; i < 12; i++) piecesBB[i] = 0ULL;
    colorBB[0] = colorBB[1] = colorBB[2] = 0ULL;
    castlingRights = 0;

    int rank = 7, file = 0, i = 0;

    while (i < fen.length() && fen[i] != ' ') {
        char c = fen[i++];
        if (c == '/') { rank--; file = 0; continue; }
        if (isdigit(c)) { file += (c - '0'); continue; }
        
        int sq = rank * 8 + file;
        switch (c) {
            case 'P': setBit(piecesBB[W_PAWN], sq); break;
            case 'N': setBit(piecesBB[W_KNIGHT], sq); break;
            case 'B': setBit(piecesBB[W_BISHOP], sq); break;
            case 'R': setBit(piecesBB[W_ROOK], sq); break;
            case 'Q': setBit(piecesBB[W_QUEEN], sq); break;
            case 'K': setBit(piecesBB[W_KING], sq); updateKingSquares(WHITE,sq); break;
            case 'p': setBit(piecesBB[B_PAWN], sq); break;
            case 'n': setBit(piecesBB[B_KNIGHT], sq); break;
            case 'b': setBit(piecesBB[B_BISHOP], sq); break;
            case 'r': setBit(piecesBB[B_ROOK], sq); break;
            case 'q': setBit(piecesBB[B_QUEEN], sq); break;
            case 'k': setBit(piecesBB[B_KING], sq); updateKingSquares(BLACK,sq); break;
        }
        file++;
    }

    i++; 
    if (i < fen.length()) {
        sideToMove = (fen[i] == 'w') ? WHITE : BLACK;
        i += 2; 
    }

    while (i < fen.length() && fen[i] != ' ') {
        switch (fen[i]) {
            case 'K': castlingRights |= WK; break;
            case 'Q': castlingRights |= WQ; break;
            case 'k': castlingRights |= BK; break;
            case 'q': castlingRights |= BQ; break;
        }
        i++;
    }
    i++;

    if (i < fen.length() && fen[i] != '-') {
        int ep_file = fen[i++] - 'a';
        int ep_rank = fen[i++] - '1';
        enPassantSq = ep_rank * 8 + ep_file;
    } else {
        enPassantSq = -1;
    }

    setupBoardArray();
    updateOccupancy();
    initZobristKey();
    initPawnKey();
    historyCount = 0;
    movesWithoutHalf = 0;
}

bool Board::makeMove(Move move) {
    GameState& current = stateStack[historyCount];
    bool isKingMoved = (boardArray[move.from] == W_KING || boardArray[move.from] == B_KING);
    current.castlingRights = this->castlingRights;
    current.enPassantSq = this->enPassantSq;
    current.oldFiftyMoveClock = movesWithoutHalf;
    movesWithoutHalf++;
    current.fiftyMoveClock = movesWithoutHalf;

    int fromSq = move.from;
    int toSq = move.to;
    int movedPiece = boardArray[fromSq];
    MoveFlag flag = move.flag;
    Color us = sideToMove; 
    Color them = (us == WHITE) ? BLACK : WHITE;
    if(movedPiece == W_PAWN || movedPiece == B_PAWN){
        pawnKey ^= zobristPieces[movedPiece][fromSq];
        pawnKey ^= zobristPieces[movedPiece][toSq];
    }

    if (move.isCapture()) {
        current.fiftyMoveClock = 0;
        movesWithoutHalf = 0;
        //historyCount = 0;
        bool enPassantLegalCheck = false;
        int victimSq = toSq;
        if (move.flag == MoveFlag::EN_PASSANT) {
            
            victimSq = (us == WHITE) ? (toSq - 8) : (toSq + 8);
            enPassantLegalCheck = true;
            
        }
        int captured = boardArray[victimSq];
        if(captured == W_PAWN || captured == B_PAWN){
            pawnKey ^= zobristPieces[captured][victimSq];
        }
        current.capturedPiece = captured; 
        popBit(piecesBB[captured], victimSq);
        boardArray[victimSq] = -1; 
        popBit(piecesBB[movedPiece], fromSq);
        setBit(piecesBB[movedPiece], toSq);
        boardArray[fromSq] = -1;
        boardArray[toSq] = movedPiece;
        if(isKingMoved) {updateKingSquares(sideToMove,toSq);}
        updateOccupancy();
        updateCastlingRights(fromSq, toSq); 
        enPassantSq = -1;
        sideToMove = them;
         zobristKey ^= zobristPieces[movedPiece][fromSq];
         zobristKey ^= zobristPieces[captured][victimSq];
         zobristKey ^= zobristPieces[movedPiece][toSq];
         zobristKey ^= zobristCastling[current.castlingRights];
         zobristKey ^= zobristCastling[castlingRights];
         zobristKey ^= zobristSide;
         if(current.enPassantSq != -1) zobristKey ^= zobristEnPassant[current.enPassantSq % 8];
        historyStack[historyCount++] = zobristKey;
         int kingSq = getLS1B(piecesBB[us == WHITE ? W_KING : B_KING]);
        //updateKingSquares(us,kingSq);
        if (GenerateMoves::isSquareAttacked(*this, kingSq, them)) {
            unmakeMove(move); 
            
            return false; 
        }

        
        return true;
    }
    if (move.isPromotion()) {
        pawnKey ^= zobristPieces[movedPiece][toSq];
        current.fiftyMoveClock = 0;
        movesWithoutHalf = 0;
        bool capture = false;
        int promotedPiece;
        int victim = -1;
        if (flag == MoveFlag::PROMOTION_QUEEN || flag == MoveFlag::CPROMOTION_QUEEN) promotedPiece = (us == WHITE) ? W_QUEEN : B_QUEEN;
        else if (flag == MoveFlag::PROMOTION_ROOK || flag == MoveFlag::CPROMOTION_ROOK) promotedPiece = (us == WHITE) ? W_ROOK : B_ROOK;
        else if (flag == MoveFlag::PROMOTION_BISHOP || flag == MoveFlag::CPROMOTION_BISHOP) promotedPiece = (us == WHITE) ? W_BISHOP : B_BISHOP;
        else promotedPiece = (us == WHITE) ? W_KNIGHT : B_KNIGHT;
        if (move.isCapturePromotion()) {
            victim = boardArray[toSq];
            current.capturedPiece = victim;
            popBit(piecesBB[victim], toSq);
            capture = true;
        }
        popBit(piecesBB[movedPiece], fromSq);
        boardArray[fromSq] = -1;
        setBit(piecesBB[promotedPiece], toSq);
        boardArray[toSq] = promotedPiece;
        if(isKingMoved) {updateKingSquares(sideToMove,toSq);}
        updateOccupancy();
        updateCastlingRights(fromSq, toSq);
        enPassantSq = -1;
        sideToMove = them;
         zobristKey ^= zobristPieces[movedPiece][fromSq];
         if(capture) zobristKey ^= zobristPieces[victim][toSq];
         zobristKey ^= zobristPieces[promotedPiece][toSq];
         zobristKey ^= zobristCastling[current.castlingRights];
         zobristKey ^= zobristCastling[castlingRights];
         zobristKey ^= zobristSide;
        if(current.enPassantSq != -1) zobristKey ^= zobristEnPassant[current.enPassantSq % 8];
        historyStack[historyCount++] = zobristKey;  
        int kingSq = getLS1B(piecesBB[us == WHITE ? W_KING : B_KING]);
        if (GenerateMoves::isSquareAttacked(*this, kingSq, them)) {
            unmakeMove(move); 
            return false; 
        }

         
        return true;
    }
    if (flag == MoveFlag::KING_CASTLE) {
        if (us == WHITE) {
            castleHappened |= 0b1000;
            popBit(piecesBB[W_ROOK], h1);
            setBit(piecesBB[W_ROOK], f1);
            boardArray[h1] = -1;
            boardArray[f1] = W_ROOK;
            zobristKey ^= zobristPieces[W_ROOK][h1];
            zobristKey ^= zobristPieces[W_ROOK][f1];
        } else {
            castleHappened |= 0b0010;
            popBit(piecesBB[B_ROOK], h8);
            setBit(piecesBB[B_ROOK], f8);
            boardArray[h8] = -1;
            boardArray[f8] = B_ROOK;
            zobristKey ^= zobristPieces[B_ROOK][h8];
            zobristKey ^= zobristPieces[B_ROOK][f8];
        }
    } 
    else if (flag == MoveFlag::QUEEN_CASTLE) {
        if (us == WHITE) {
            castleHappened |= 0b0100;
            popBit(piecesBB[W_ROOK], a1);
            setBit(piecesBB[W_ROOK], d1);
            boardArray[a1] = -1;
            boardArray[d1] = W_ROOK;
            zobristKey ^= zobristPieces[W_ROOK][d1];
            zobristKey ^= zobristPieces[W_ROOK][a1];

        } else {
            castleHappened |= 0b0001;
            popBit(piecesBB[B_ROOK], a8);
            setBit(piecesBB[B_ROOK], d8);
            boardArray[a8] = -1;
            boardArray[d8] = B_ROOK;
            zobristKey ^= zobristPieces[B_ROOK][d8];
            zobristKey ^= zobristPieces[B_ROOK][a8];
        }
    }
    popBit(piecesBB[movedPiece], fromSq);
    setBit(piecesBB[movedPiece], toSq);

    boardArray[toSq] = movedPiece;
    boardArray[fromSq] = -1;
    if(isKingMoved) {updateKingSquares(sideToMove,toSq);}
    updateOccupancy();
    updateCastlingRights(fromSq, toSq);
    if(movedPiece == W_PAWN || movedPiece == B_PAWN) {current.fiftyMoveClock = 0; movesWithoutHalf = 0;}
    if (flag == MoveFlag::DOUBLE_PAWN_PUSH) {
    enPassantSq = (us == WHITE) ? (toSq - 8) : (toSq + 8);
    } else {
        enPassantSq = -1;
    }
    sideToMove = them;
     zobristKey ^= zobristPieces[movedPiece][fromSq];
     zobristKey ^= zobristPieces[movedPiece][toSq];
     zobristKey ^= zobristCastling[current.castlingRights];
     zobristKey ^= zobristCastling[castlingRights];
     zobristKey ^= zobristSide;
     if(current.enPassantSq != -1) zobristKey ^= zobristEnPassant[current.enPassantSq % 8];
     if(enPassantSq != -1) zobristKey ^= zobristEnPassant[enPassantSq % 8];

    int kingSq = getLS1B(piecesBB[us == WHITE ? W_KING : B_KING]);
    historyStack[historyCount++] = zobristKey; 
    if (GenerateMoves::isSquareAttacked(*this, kingSq, them)) {
    unmakeMove(move); 
    return false; 
    }
    return true; 
}

void Board::updateCastlingRights(int fromSq, int toSq) {
    if (fromSq == e1) castlingRights &= 0b0011;
    if (fromSq == e8) castlingRights &= 0b1100;

    if (fromSq == a1 || toSq == a1) castlingRights &= 0b1011;
    else if (fromSq == h1 || toSq == h1) castlingRights &= 0b0111;
    
    if (fromSq == a8 || toSq == a8) castlingRights &= 0b1110;
    else if (fromSq == h8 || toSq == h8) castlingRights &= 0b1101;
}

void Board::unmakeMove(Move move){
    bool isKingMoved = (boardArray[move.to] == W_KING || boardArray[move.to] == B_KING);
    GameState& gameStatePrev = stateStack[historyCount-1];
    MoveFlag flag = move.flag;
    int toSq = move.to;
    int fromSq = move.from;
    int movedPiece = boardArray[toSq];
    int capturedPiece = -1;
    Color us = (movedPiece <= W_KING) ? WHITE : BLACK;
    Color them = (sideToMove == WHITE) ? BLACK : WHITE;
    if(move.isCapture() || move.isCapturePromotion()) capturedPiece = gameStatePrev.capturedPiece;

    if(flag == MoveFlag::QUIET || flag == MoveFlag::DOUBLE_PAWN_PUSH){
        popBit(piecesBB[movedPiece],toSq);
        setBit(piecesBB[movedPiece],fromSq);
        boardArray[toSq] = -1;
        boardArray[fromSq] = movedPiece;
        zobristKey ^= zobristPieces[movedPiece][fromSq];
        zobristKey ^= zobristPieces[movedPiece][toSq];
        if (movedPiece == W_PAWN || movedPiece == B_PAWN) {
            pawnKey ^= zobristPieces[movedPiece][fromSq];
            pawnKey ^= zobristPieces[movedPiece][toSq];
        }
    }
    else if (move.isCastle()) {
        castleHappened = 0b000;
        popBit(piecesBB[movedPiece], toSq);
        setBit(piecesBB[movedPiece], fromSq);
        boardArray[toSq] = -1;
        boardArray[fromSq] = movedPiece;

         zobristKey ^= zobristPieces[movedPiece][fromSq];
         zobristKey ^= zobristPieces[movedPiece][toSq];

        if (toSq == g1) {
            popBit(piecesBB[W_ROOK], f1);
            setBit(piecesBB[W_ROOK], h1);
            boardArray[f1] = -1;
            boardArray[h1] = W_ROOK;
             zobristKey ^= zobristPieces[W_ROOK][f1]; 
             zobristKey ^= zobristPieces[W_ROOK][h1]; 
        } 
        else if (toSq == c1) { 
            popBit(piecesBB[W_ROOK], d1);
            setBit(piecesBB[W_ROOK], a1);
            boardArray[d1] = -1;
            boardArray[a1] = W_ROOK;
             zobristKey ^= zobristPieces[W_ROOK][d1];
             zobristKey ^= zobristPieces[W_ROOK][a1];
        }
        else if (toSq == g8) { 
            popBit(piecesBB[B_ROOK], f8);
            setBit(piecesBB[B_ROOK], h8);
            boardArray[f8] = -1;
            boardArray[h8] = B_ROOK;
             zobristKey ^= zobristPieces[B_ROOK][f8];
             zobristKey ^= zobristPieces[B_ROOK][h8];
        }
        else if (toSq == c8) { 
            popBit(piecesBB[B_ROOK], d8);
            setBit(piecesBB[B_ROOK], a8);
            boardArray[d8] = -1;
            boardArray[a8] = B_ROOK;
             zobristKey ^= zobristPieces[B_ROOK][d8];
             zobristKey ^= zobristPieces[B_ROOK][a8];
        }
    }
    else if (move.isCapturePromotion()) {
        int originalPawn = (us == WHITE) ? W_PAWN : B_PAWN;
        int promotedPiece = boardArray[toSq];
        popBit(piecesBB[promotedPiece], toSq);
        setBit(piecesBB[originalPawn], fromSq);
        boardArray[fromSq] = originalPawn;
        int victim = gameStatePrev.capturedPiece;
        setBit(piecesBB[victim], toSq);
        boardArray[toSq] = victim;
         zobristKey ^= zobristPieces[promotedPiece][toSq];
         zobristKey ^= zobristPieces[victim][toSq];
         zobristKey ^= zobristPieces[originalPawn][fromSq];
         pawnKey ^= zobristPieces[originalPawn][fromSq];
        
        if (victim == W_PAWN || victim == B_PAWN) {
            pawnKey ^= zobristPieces[victim][toSq];
        }
    }
    else if (move.isQuietPromotion()) {
        int promotedPiece = boardArray[toSq]; 
        popBit(piecesBB[promotedPiece], toSq);
        boardArray[toSq] = -1;
        int originalPawn = (us == WHITE) ? W_PAWN : B_PAWN;
        setBit(piecesBB[originalPawn], fromSq);
        boardArray[fromSq] = originalPawn;
        zobristKey ^= zobristPieces[promotedPiece][toSq];
        zobristKey ^= zobristPieces[originalPawn][fromSq];
        pawnKey ^= zobristPieces[originalPawn][fromSq];
    }

    else if (move.isCapture()) {
        
        popBit(piecesBB[movedPiece], toSq);
        setBit(piecesBB[movedPiece], fromSq);
        boardArray[fromSq] = movedPiece;

        int victim = gameStatePrev.capturedPiece;
        if (flag == MoveFlag::EN_PASSANT) {
            

            int victimSq = (us == WHITE) ? (toSq - 8) : (toSq + 8);
            setBit(piecesBB[victim], victimSq);
            boardArray[victimSq] = victim;
            boardArray[toSq] = -1;
        } else {
            setBit(piecesBB[victim], toSq);
            boardArray[toSq] = victim;
        }
        
         zobristKey ^= zobristPieces[movedPiece][toSq];
         zobristKey ^= zobristPieces[movedPiece][fromSq];
         int victimSq = (flag == MoveFlag::EN_PASSANT) ? ((us == WHITE) ? (toSq - 8) : (toSq + 8)) : toSq;
        zobristKey ^= zobristPieces[victim][victimSq];
        if (movedPiece == W_PAWN || movedPiece == B_PAWN) {
            pawnKey ^= zobristPieces[movedPiece][fromSq];
            pawnKey ^= zobristPieces[movedPiece][toSq];
        }
        if(capturedPiece == W_PAWN || capturedPiece == B_PAWN){
            pawnKey ^= zobristPieces[victim][victimSq];}
    }
    sideToMove = us;
     zobristKey ^= zobristCastling[castlingRights];
     zobristKey ^= zobristCastling[gameStatePrev.castlingRights];
    
    if (enPassantSq != -1)           zobristKey ^= zobristEnPassant[enPassantSq % 8];
    if (gameStatePrev.enPassantSq != -1) zobristKey ^= zobristEnPassant[gameStatePrev.enPassantSq % 8];
     zobristKey ^= zobristSide;
    castlingRights = gameStatePrev.castlingRights;
    enPassantSq = gameStatePrev.enPassantSq;
    gameState.fiftyMoveClock= gameStatePrev.oldFiftyMoveClock;
    movesWithoutHalf = gameStatePrev.oldFiftyMoveClock;
    if(isKingMoved) {updateKingSquares(sideToMove,fromSq);}
    if(historyCount>0) historyCount--;
    updateOccupancy();
    uint64_t key = calculateZobristFromScratch();
    //if(zobristKey == key) std::cout<<"IT IS FINE" <<std::endl;
    //else std::cout<<"Something is wrong. KEY CALC: "<<zobristKey<<" KEY SCRATCH: "<<key <<std::endl; 
}

uint64_t Board::Perft(int depth, GenerateMoves& gen)
{
    Move move_list[256];
    int i = gen.generateMoves(*this, move_list);
    uint64_t nodes = 0;
    if (depth == 1) {
        for (int j = 0; j < i; ++j) {
            if (makeMove(move_list[j])) {
                nodes++;
                unmakeMove(move_list[j]);
            }
        }
        return nodes;
    }

    for (int j = 0; j < i; ++j) {
        if (makeMove(move_list[j])) {
            nodes += Perft(depth - 1, gen);
            unmakeMove(move_list[j]);
        }
    }

    return nodes;
}

int Board::evaluate(){
    return EngineEval::evaluate(*this);
}

bool Board::isEndGame(){
    return EngineEval::isEndGame(*this);
}

int Board::getPhase(){
    return EngineEval::getPhase(*this);
}

void Board::updateKingSquares(Color color,int sq){
    if(color == WHITE) whiteKingSq = sq;
    else blackKingSq = sq;
}

bool Board::isInCheck(){
    
    Color attackerColor = (sideToMove == WHITE) ? BLACK : WHITE;
    int sq = (sideToMove == WHITE) ? whiteKingSq : blackKingSq;
    return GenerateMoves::isSquareAttacked(*this, sq, attackerColor);

}

void Board::initPawnKey(){
    pawnKey = 1;
    for (int i = 0; i < 64; i++) {
        int piece = boardArray[i];
        if (piece == W_PAWN || piece == B_PAWN ) pawnKey ^= zobristPieces[piece][i];
    }


}

void Board::initZobristKey(){
    zobristKey = 1;
    for (int i = 0; i < 64; i++) {
     int piece = boardArray[i];
    if (piece != -1) zobristKey ^= zobristPieces[piece][i];
    }
    if (sideToMove == BLACK) zobristKey ^= zobristSide;
    zobristKey ^= zobristCastling[castlingRights];
    if (enPassantSq != -1) zobristKey ^= zobristEnPassant[enPassantSq % 8];
}

uint64_t Board::calculateZobristFromScratch() {
    uint64_t newKey = 1;
    for (int i = 0; i < 64; i++) {
        int piece = boardArray[i];
        if (piece == W_PAWN || piece == B_PAWN ) newKey ^= zobristPieces[piece][i];
    }
    return newKey;
}

bool Board::isRepetition() {
    if (historyCount < 4) return false;
    int limit = std::max(0, historyCount-gameState.fiftyMoveClock);
    for (int i = historyCount - 3; i >= limit; i -= 2) {
        if (historyStack[i] == zobristKey) {
            
            return true;
            
        }
    }
    return false;
}

bool Board::isZugzwang() const {
    if (sideToMove == WHITE) {
        return (getPieceBB(W_BISHOP) | getPieceBB(W_KNIGHT) | getPieceBB(W_QUEEN) | getPieceBB(W_ROOK)) == 0;
    } else {

        return (getPieceBB(B_BISHOP) | getPieceBB(B_KNIGHT) | getPieceBB(B_QUEEN) | getPieceBB(B_ROOK)) == 0;
    }
}

void Board::makeNullMove() {

    GameState& current = stateStack[historyCount];
    current.castlingRights = this->castlingRights;
    current.enPassantSq = this->enPassantSq;
    current.oldFiftyMoveClock = this->movesWithoutHalf;
    current.capturedPiece = -1; 

    if (enPassantSq != -1) {
        zobristKey ^= zobristEnPassant[enPassantSq % 8];
        enPassantSq = -1; 
    }

    zobristKey ^= zobristSide;
    sideToMove = (sideToMove == WHITE) ? BLACK : WHITE;

    historyStack[historyCount++] = zobristKey;
    movesWithoutHalf++; 
}

void Board::unmakeNullMove() {
    historyCount--;
    GameState& prev = stateStack[historyCount];
    
    sideToMove = (sideToMove == WHITE) ? BLACK : WHITE;
    zobristKey ^= zobristSide;

    enPassantSq = prev.enPassantSq;
    if (enPassantSq != -1) {
        zobristKey ^= zobristEnPassant[enPassantSq % 8];
    }

    movesWithoutHalf = prev.oldFiftyMoveClock;
    castlingRights = prev.castlingRights;
}

bool Board::SEECalculation(const Board& board, int sq, Color attackerColor) {
    int gain[32];
    int tradeCount = 0;
    uint64_t currOccupancy = board.getOccupancy(BOTH);

    int targetPiece = getPieceType(sq);
    gain[0] = PIECE_VALUES[targetPiece];
    
    Color sideToMove = attackerColor;

    while (true) {
        int lvaSq = GenerateMoves::findLVA(board, sq, sideToMove, currOccupancy);
        if (lvaSq == -1) break;

        int pieceType = getPieceType(lvaSq);
        tradeCount++;
        gain[tradeCount] = PIECE_VALUES[pieceType] - gain[tradeCount - 1];
        currOccupancy &= ~(1ULL << lvaSq);
        sideToMove = (sideToMove == WHITE) ? BLACK : WHITE;
        if (pieceType == W_KING || pieceType == B_KING || tradeCount >= 31) break;
    }

    while (--tradeCount > 0) {
        gain[tradeCount - 1] = -std::max(-gain[tradeCount - 1], gain[tradeCount]);
    }

    return gain[0] >= 0;
}

bool Board::isLegalMove(Move move) {
    Move moveList[256];
    int moveCount = GenerateMoves::generateMoves(*this, moveList);
    
    bool inArray = false;
    for (int i = 0; i < moveCount; i++) {
        if (moveList[i] == move) {
            inArray = true;
            break;
        }
    }
    
    if (!inArray) return false;

    bool isHappened = makeMove(move);
    if (isHappened) {
        unmakeMove(move);
        return true;
    }

    return false;
}