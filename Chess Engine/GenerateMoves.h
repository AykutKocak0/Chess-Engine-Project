#pragma once
#include <stdint.h>
#include <vector>
#include "Types.h"
#include "Board.h"
#include <string>

class GenerateMoves {
    private:
        static bool initialized;
        static uint64_t knightMoves[64];
        static uint64_t kingMoves[64];
        static uint64_t pawnAttacks[2][64];
        static Magic rookMagics[64];
        static Magic bishopMagics[64];
        static uint64_t rookTable[0x19000];  
        static uint64_t bishopTable[0x1480]; 
        static uint64_t rmask(int sq);
        static uint64_t bmask(int sq);
        static uint64_t ratt(int sq, uint64_t block);
        static uint64_t batt(int sq, uint64_t block);
        static uint64_t index_to_uint64(int index, int bits, uint64_t m); 
        static uint64_t genKnightPlaces(int sq);
        static uint64_t genPawnAttacks(int sq, Color color);
        static uint64_t genKingPlaces(int sq);

        static void generateBishopMoves(const Board& board, Move* moveList, Color color, int& moveCount);
        static void generateQueenMoves(const Board& board, Move* moveList, Color color, int& moveCount);
        static void generateRookMoves(const Board& board, Move* moveList, Color color, int& moveCount);
        static void generatePawnMoves(const Board& board, Move* moveList, Color color, int& moveCount);
        static void generateKnightMoves(const Board& board, Move* moveList, Color color, int& moveCount);
        static void generateKingMoves(const Board& board, Move* moveList, Color color, int& moveCount);

        void generateBishopCaptures(const Board& board, Move* moveList, Color color, int& moveCount);
        void generateQueenCaptures(const Board& board, Move* moveList, Color color, int& moveCount);
        void generateRookCaptures(const Board& board, Move* moveList, Color color, int& moveCount);
        void generatePawnCaptures(const Board& board, Move* moveList, Color color, int& moveCount);
        void generateKnightCaptures(const Board& board, Move* moveList, Color color, int& moveCount);
        void generateKingCaptures(const Board& board, Move* moveList, Color color, int& moveCount);

        void generateCheckKnightMoves(const Board& board, Move* quietCheckMoveList, Color color, int& moveCount);
        void generateCheckBishopMoves(const Board& board, Move* quietCheckMoveList, Color color, int& moveCount);
        void generateCheckRookMoves(const Board& board, Move* quietCheckMoveList, Color color, int& moveCount);
        void generateCheckQueenMoves(const Board& board, Move* quietCheckMoveList, Color color, int& moveCount);
        void generateCheckPawnMoves(const Board& board, Move* quietCheckMoveList, Color color, int& moveCount);
        

    public:
        GenerateMoves();
        static bool isSquareAttacked(const Board& board, int sq, Color color);
        static int findLVA(const Board& board, int sq, Color attackColor, uint64_t occupancy);
        static bool SEECalculation(const Board& board, int sq, int attackerColor);

        std::string squareToString(int sq);
        void printMoveList(const std::vector<Move>& moveList);
        static void init();
        static int generateMoves(const Board& board, Move* moveList);
        int generateCaptureMoves(const Board& board, Move* moveList, Color color);
        int generateQuietCheckMoves(const Board& board, Move* moveList, Color color, int moveCount);
        static uint64_t getKnightMoves(int sq){return knightMoves[sq];}
        static Magic getRookMagic(int sq){return rookMagics[sq];}
        static Magic getBishopMagic(int sq){return bishopMagics[sq];}
        static uint64_t getRookMagicBB(int sq, uint64_t occupancy);
        static uint64_t getBishopMagicBB(int sq, uint64_t occupancy);


};