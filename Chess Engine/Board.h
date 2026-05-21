#pragma once
#include <stdint.h>
#include "Types.h"
#include <string>

class GenerateMoves;

class Board{
public:
    Board();
    bool isEndGame();
    int getPhase();
    void clear();
    void addPiece(PieceType pType, int sq);
    Color sideToMove;
    int enPassantSq;
    int castlingRights;
    GameState gameState;
    int historyCount;
    int castleHappened;
    uint64_t historyStack[1024];
    void printBoard();
    bool makeMove(Move move);
    void unmakeMove(Move move);
    void setupStartingBoard();
    uint64_t getOccupancy(Color color) const;
    uint64_t getPieceBB(PieceType pT) const;
    int getPieceType(int sq) const;
    int getBoardHistory() {return historyCount;}
    GameState stateStack[1024];

    int getKingSquare(Color color) const;
    int getMovesWH() {return movesWithoutHalf;}
    void parseFEN(const std::string& fen);
    uint64_t Perft(int depth, GenerateMoves& gen);
    int evaluate();
    bool isInCheck();
    uint64_t getZobristKey() const;
    uint64_t getPawnKey() const;
    bool isRepetition();
    uint64_t calculateZobristFromScratch();
    bool isZugzwang() const;
    void makeNullMove();
    void unmakeNullMove();
    bool SEECalculation(const Board& board,int sq, Color attackerColor);
    void removeAttackerOcc(uint64_t &occupancy, int attackerType);
    bool isLegalMove(Move move);


private:
    
    
    int movesWithoutHalf;
    uint64_t zobristKey;
    uint64_t pawnKey;
    
    int whiteKingSq;
    int blackKingSq;
    int boardArray[64];
    uint64_t piecesBB[12];
    uint64_t colorBB[3];
    void updateOccupancy();
    void setupBoardArray();
    void updateCastlingRights(int fromSq, int toSq);
    void updateKingSquares(Color color, int sq);
    void initZobristKey();
    void initPawnKey();
    
};