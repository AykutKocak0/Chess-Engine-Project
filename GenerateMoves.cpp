#include <stdint.h>
#include "Types.h"
#include "Board.h"
#include "GenerateMoves.h"
#include <vector>
#include <string>
#include <iostream>
#include "EngineEval.h"
const uint64_t NOT_A_FILE = 0xfefefefefefefefeULL;
const uint64_t NOT_H_FILE = 0x7f7f7f7f7f7f7f7fULL;
const uint64_t NOT_AB_FILE = 0xfcfcfcfcfcfcfcfcULL;
const uint64_t NOT_HG_FILE = 0x3f3f3f3f3f3f3f3fULL;
const uint64_t RANK_1 = 0x00000000000000FFULL;
const uint64_t RANK_4 = 0x00000000FF000000ULL;
const uint64_t RANK_5 = 0x000000FF00000000ULL;
const uint64_t RANK_8 = 0xFF00000000000000ULL;
bool GenerateMoves::initialized = false;
uint64_t GenerateMoves::knightMoves[64];
uint64_t GenerateMoves::kingMoves[64];
uint64_t GenerateMoves::pawnAttacks[2][64];
Magic GenerateMoves::rookMagics[64];
Magic GenerateMoves::bishopMagics[64];
uint64_t GenerateMoves::rookTable[0x19000];
uint64_t GenerateMoves::bishopTable[0x1480];
const int BBits[64] = {
  6, 5, 5, 5, 5, 5, 5, 6,
  5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 7, 7, 7, 7, 5, 5,
  5, 5, 7, 9, 9, 7, 5, 5,
  5, 5, 7, 9, 9, 7, 5, 5,
  5, 5, 7, 7, 7, 7, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5,
  6, 5, 5, 5, 5, 5, 5, 6
};
const int RBits[64] = {
  12, 11, 11, 11, 11, 11, 11, 12,
  11, 10, 10, 10, 10, 10, 10, 11,
  11, 10, 10, 10, 10, 10, 10, 11,
  11, 10, 10, 10, 10, 10, 10, 11,
  11, 10, 10, 10, 10, 10, 10, 11,
  11, 10, 10, 10, 10, 10, 10, 11,
  11, 10, 10, 10, 10, 10, 10, 11,
  12, 11, 11, 11, 11, 11, 11, 12
};
const uint64_t RMagic[64] = {
  0xa8002c000108020ULL,
  0x6c00049b0002001ULL,
  0x100200010090040ULL,
  0x2480041000800801ULL,
  0x280028004000800ULL,
  0x900410008040022ULL,
  0x280020001001080ULL,
  0x2880002041000080ULL,
  0xa000800080400034ULL,
  0x4808020004000ULL,
  0x2290802004801000ULL,
  0x411000d00100020ULL,
  0x402800800040080ULL,
  0xb000401004208ULL,
  0x2409000100040200ULL,
  0x1002100004082ULL,
  0x22878001e24000ULL,
  0x1090810021004010ULL,
  0x801030040200012ULL,
  0x500808008001000ULL,
  0xa08018014000880ULL,
  0x8000808004000200ULL,
  0x201008080010200ULL,
  0x801020000441091ULL,
  0x800080204005ULL,
  0x1040200040100048ULL,
  0x120200402082ULL,
  0xd14880480100080ULL,
  0x12040280080080ULL,
  0x100040080020080ULL,
  0x9020010080800200ULL,
  0x813241200148449ULL,
  0x491604001800080ULL,
  0x100401000402001ULL,
  0x4820010021001040ULL,
  0x400402202000812ULL,
  0x209009005000802ULL,
  0x810800601800400ULL,
  0x4301083214000150ULL,
  0x204026458e001401ULL,
  0x40204000808000ULL,
  0x8001008040010020ULL,
  0x8410820820420010ULL,
  0x1003001000090020ULL,
  0x804040008008080ULL,
  0x12000810020004ULL,
  0x1000100200040208ULL,
  0x430000a044020001ULL,
  0x280009023410300ULL,
  0xe0100040002240ULL,
  0x200100401700ULL,
  0x2244100408008080ULL,
  0x8000400801980ULL,
  0x2000810040200ULL,
  0x8010100228810400ULL,
  0x2000009044210200ULL,
  0x4080008040102101ULL,
  0x40002080411d01ULL,
  0x2005524060000901ULL,
  0x502001008400422ULL,
  0x489a000810200402ULL,
  0x1004400080a13ULL,
  0x4000011008020084ULL,
  0x26002114058042ULL,
};
const uint64_t BMagic[64] = {
  0x89a1121896040240ULL,
  0x2004844802002010ULL,
  0x2068080051921000ULL,
  0x62880a0220200808ULL,
  0x4042004000000ULL,
  0x100822020200011ULL,
  0xc00444222012000aULL,
  0x28808801216001ULL,
  0x400492088408100ULL,
  0x201c401040c0084ULL,
  0x840800910a0010ULL,
  0x82080240060ULL,
  0x2000840504006000ULL,
  0x30010c4108405004ULL,
  0x1008005410080802ULL,
  0x8144042209100900ULL,
  0x208081020014400ULL,
  0x4800201208ca00ULL,
  0xf18140408012008ULL,
  0x1004002802102001ULL,
  0x841000820080811ULL,
  0x40200200a42008ULL,
  0x800054042000ULL,
  0x88010400410c9000ULL,
  0x520040470104290ULL,
  0x1004040051500081ULL,
  0x2002081833080021ULL,
  0x400c00c010142ULL,
  0x941408200c002000ULL,
  0x658810000806011ULL,
  0x188071040440a00ULL,
  0x4800404002011c00ULL,
  0x104442040404200ULL,
  0x511080202091021ULL,
  0x4022401120400ULL,
  0x80c0040400080120ULL,
  0x8040010040820802ULL,
  0x480810700020090ULL,
  0x102008e00040242ULL,
  0x809005202050100ULL,
  0x8002024220104080ULL,
  0x431008804142000ULL,
  0x19001802081400ULL,
  0x200014208040080ULL,
  0x3308082008200100ULL,
  0x41010500040c020ULL,
  0x4012020c04210308ULL,
  0x208220a202004080ULL,
  0x111040120082000ULL,
  0x6803040141280a00ULL,
  0x2101004202410000ULL,
  0x8200000041108022ULL,
  0x21082088000ULL,
  0x2410204010040ULL,
  0x40100400809000ULL,
  0x822088220820214ULL,
  0x40808090012004ULL,
  0x910224040218c9ULL,
  0x402814422015008ULL,
  0x90014004842410ULL,
  0x1000042304105ULL,
  0x10008830412a00ULL,
  0x2520081090008908ULL,
  0x40102000a0a60140ULL,
};
void GenerateMoves::init(){
    for(int i = 0;i<64;++i){
        kingMoves[i] = genKingPlaces(i);
        knightMoves[i] = genKnightPlaces(i);
        pawnAttacks[0][i] = genPawnAttacks(i,WHITE);
        pawnAttacks[1][i] = genPawnAttacks(i,BLACK);

    }
    int rookOffset = 0;
    int bishopOffset = 0;

    for (int sq = 0; sq < 64; sq++) {
        rookMagics[sq].mask = rmask(sq);
        rookMagics[sq].magic = RMagic[sq];
        rookMagics[sq].shift = 64 - RBits[sq];
        rookMagics[sq].ptr = &rookTable[rookOffset];

        int rookSize = 1 << RBits[sq];
        for (int i = 0; i < rookSize; i++) {
            uint64_t occ = index_to_uint64(i, RBits[sq], rookMagics[sq].mask);
            uint64_t magicIdx = (occ * rookMagics[sq].magic) >> rookMagics[sq].shift;
            rookMagics[sq].ptr[magicIdx] = ratt(sq, occ);
        }
        rookOffset += rookSize;
        bishopMagics[sq].mask = bmask(sq);
        bishopMagics[sq].magic = BMagic[sq];
        bishopMagics[sq].shift = 64 - BBits[sq];
        bishopMagics[sq].ptr = &bishopTable[bishopOffset];

        int bishopSize = 1 << BBits[sq];
        for (int i = 0; i < bishopSize; i++) {
            uint64_t occ = index_to_uint64(i, BBits[sq], bishopMagics[sq].mask);
            uint64_t magicIdx = (occ * bishopMagics[sq].magic) >> bishopMagics[sq].shift;
            bishopMagics[sq].ptr[magicIdx] = batt(sq, occ);
        }
        bishopOffset += bishopSize;
    }
}

GenerateMoves::GenerateMoves(){
    if(!initialized){
        GenerateMoves::init();
        initialized = true;
    }
}
uint64_t GenerateMoves::rmask(int sq) {
    uint64_t result = 0ULL;
    int rk = sq / 8, fl = sq % 8, r, f;
    for (r = rk + 1; r <= 6; r++) result |= (1ULL << (fl + r * 8));
    for (r = rk - 1; r >= 1; r--) result |= (1ULL << (fl + r * 8));
    for (f = fl + 1; f <= 6; f++) result |= (1ULL << (f + rk * 8));
    for (f = fl - 1; f >= 1; f--) result |= (1ULL << (f + rk * 8));
    return result;
}
uint64_t GenerateMoves::bmask(int sq) {
    uint64_t result = 0ULL;
    int rk = sq / 8, fl = sq % 8, r, f;
    for (r = rk + 1, f = fl + 1; r <= 6 && f <= 6; r++, f++) result |= (1ULL << (f + r * 8));
    for (r = rk + 1, f = fl - 1; r <= 6 && f >= 1; r++, f--) result |= (1ULL << (f + r * 8));
    for (r = rk - 1, f = fl + 1; r >= 1 && f <= 6; r--, f++) result |= (1ULL << (f + r * 8));
    for (r = rk - 1, f = fl - 1; r >= 1 && f >= 1; r--, f--) result |= (1ULL << (f + r * 8));
    return result;
}
uint64_t GenerateMoves::ratt(int sq, uint64_t block) {
    uint64_t result = 0ULL;
    int rk = sq / 8, fl = sq % 8, r, f;
    for (r = rk + 1; r <= 7; r++) {
        result |= (1ULL << (fl + r * 8));
        if (block & (1ULL << (fl + r * 8))) break;
    }
    for (r = rk - 1; r >= 0; r--) {
        result |= (1ULL << (fl + r * 8));
        if (block & (1ULL << (fl + r * 8))) break;
    }
    for (f = fl + 1; f <= 7; f++) {
        result |= (1ULL << (f + rk * 8));
        if (block & (1ULL << (f + rk * 8))) break;
    }
    for (f = fl - 1; f >= 0; f--) {
        result |= (1ULL << (f + rk * 8));
        if (block & (1ULL << (f + rk * 8))) break;
    }
    return result;
}
uint64_t GenerateMoves::batt(int sq, uint64_t block) {
  uint64_t result = 0ULL;
  int rk = sq/8, fl = sq%8, r, f;
  for(r = rk+1, f = fl+1; r <= 7 && f <= 7; r++, f++) {
    result |= (1ULL << (f + r*8));
    if(block & (1ULL << (f + r * 8))) break;
  }
  for(r = rk+1, f = fl-1; r <= 7 && f >= 0; r++, f--) {
    result |= (1ULL << (f + r*8));
    if(block & (1ULL << (f + r * 8))) break;
  }
  for(r = rk-1, f = fl+1; r >= 0 && f <= 7; r--, f++) {
    result |= (1ULL << (f + r*8));
    if(block & (1ULL << (f + r * 8))) break;
  }
  for(r = rk-1, f = fl-1; r >= 0 && f >= 0; r--, f--) {
    result |= (1ULL << (f + r*8));
    if(block & (1ULL << (f + r * 8))) break;
  }
  return result;
}
uint64_t GenerateMoves::index_to_uint64(int index, int bits, uint64_t mask) {
    uint64_t result = 0ULL;
    for (int i = 0; i < bits; i++) {
        int sq = getLS1B(mask);
        popBit(mask, sq);     
        if (index & (1 << i)) {
            result |= (1ULL << sq);
        }
    }
    return result;
}
uint64_t GenerateMoves::genKnightPlaces(int sq){
    uint64_t knightLoc = (1ULL << sq);
    uint64_t attacked = 0ULL;
    attacked |= (knightLoc << 17) & NOT_A_FILE;
    attacked |= (knightLoc << 10) & NOT_AB_FILE;
    attacked |= (knightLoc  >> 6)  & NOT_AB_FILE;
    attacked |= (knightLoc >> 15) & NOT_A_FILE;
    attacked |= (knightLoc  << 15) & NOT_H_FILE;
    attacked |= (knightLoc  << 6)  & NOT_HG_FILE;
    attacked |= (knightLoc  >> 10) & NOT_HG_FILE;
    attacked |= (knightLoc  >> 17) & NOT_H_FILE;

    return attacked;
}
uint64_t GenerateMoves::genKingPlaces(int sq){
    uint64_t kingLoc = (1ULL << sq);
    uint64_t attacked = 0ULL;
    attacked |= (kingLoc >> 1) & NOT_H_FILE;
    attacked |= (kingLoc << 1) & NOT_A_FILE;
    attacked |= (kingLoc >> 9) & NOT_H_FILE;
    attacked |= (kingLoc << 9) & NOT_A_FILE;
    attacked |= (kingLoc >> 7) & NOT_A_FILE;
    attacked |= (kingLoc << 7) & NOT_H_FILE;
    attacked |= (kingLoc << 8);
    attacked |= (kingLoc >> 8);

    return attacked;
}
uint64_t GenerateMoves::genPawnAttacks(int sq,Color color) {
    uint64_t pawnLoc = (1ULL << sq);
    uint64_t attacked = 0ULL;

    if (color == WHITE) {
        attacked |= (pawnLoc << 7) & NOT_H_FILE; 
        attacked |= (pawnLoc << 9) & NOT_A_FILE; 
    } else {
        attacked |= (pawnLoc >> 7) & NOT_A_FILE; 
        attacked |= (pawnLoc >> 9) & NOT_H_FILE; 
    }
    return attacked;
}

int GenerateMoves::generateMoves(const Board& board, Move* moveList){
    Color side = board.sideToMove;
    int moveCount = 0;
    generatePawnMoves(board, moveList, side, moveCount);
    generateKnightMoves(board, moveList, side, moveCount);
    generateBishopMoves(board, moveList, side, moveCount);
    generateRookMoves(board, moveList, side, moveCount);
    generateQueenMoves(board, moveList, side, moveCount);   
    generateKingMoves(board, moveList, side, moveCount);
    return moveCount;
}
int GenerateMoves::generateCaptureMoves(const Board& board, Move* moveList, Color color) {
    int moveCount = 0;
    generatePawnCaptures(board, moveList, color, moveCount);
    generateKnightCaptures(board, moveList, color, moveCount);
    generateBishopCaptures(board, moveList, color, moveCount);
    generateRookCaptures(board, moveList, color, moveCount);
    generateQueenCaptures(board, moveList, color, moveCount);
    generateKingCaptures(board, moveList, color, moveCount);
    return moveCount;
}
int GenerateMoves::generateQuietCheckMoves(const Board& board, Move* moveList, Color color, int moveCount){
    generateCheckPawnMoves(board, moveList, color, moveCount);
    generateCheckKnightMoves(board, moveList, color, moveCount);
    generateCheckBishopMoves(board, moveList, color, moveCount);
    generateCheckRookMoves(board, moveList, color, moveCount);
    generateCheckQueenMoves(board, moveList, color, moveCount);
    return moveCount;
}

void GenerateMoves::generatePawnMoves(const Board& board, Move* moveList, Color color, int& moveCount) {
    uint64_t pawns = board.getPieceBB(color == WHITE ? W_PAWN : B_PAWN);
    uint64_t empty = ~board.getOccupancy(BOTH);
    uint64_t enemies = board.getOccupancy(color == WHITE ? BLACK : WHITE);
    int shift = (color == WHITE) ? 8 : -8;
    uint64_t dblPushRank = (color == WHITE) ? RANK_4 : RANK_5;
    int promBoundary = (color == WHITE) ? 56 : 7;
    uint64_t singlePushes = (color == WHITE) ? (pawns << 8) & empty : (pawns >> 8) & empty;
    uint64_t doublePushes = (color == WHITE) ? (singlePushes << 8) & empty & dblPushRank 
                                          : (singlePushes >> 8) & empty & dblPushRank;
    uint64_t tempPushes = singlePushes;
    while (tempPushes) {
        int toSq = getLS1B(tempPushes);
        int fromSq = toSq - shift;
        bool isPromotion = (color == WHITE) ? (toSq >= 56) : (toSq <= 7);
        if (isPromotion) {
            moveList[moveCount++]=(Move(fromSq, toSq, MoveFlag::PROMOTION_QUEEN));
            moveList[moveCount++]=(Move(fromSq, toSq, MoveFlag::PROMOTION_ROOK));
            moveList[moveCount++]=(Move(fromSq, toSq, MoveFlag::PROMOTION_BISHOP));
            moveList[moveCount++]=(Move(fromSq, toSq, MoveFlag::PROMOTION_KNIGHT));
        } else {
            moveList[moveCount++]=(Move(fromSq, toSq, MoveFlag::QUIET));
        }
        popBit(tempPushes, toSq);
    }
    uint64_t tempDouble = doublePushes;
    while (tempDouble) {
        int toSq = getLS1B(tempDouble);
        moveList[moveCount++]=(Move(toSq - (shift * 2), toSq, MoveFlag::DOUBLE_PAWN_PUSH));
        popBit(tempDouble, toSq);
    }
    uint64_t tempPawns = pawns;
    while (tempPawns) {
        int fromSq = getLS1B(tempPawns);
        uint64_t attacks = pawnAttacks[color][fromSq] & enemies;

        while (attacks) {
            int toSq = getLS1B(attacks);
            bool isPromotion = (color == WHITE) ? (toSq >= 56) : (toSq <= 7);
            
            if (isPromotion) {
                moveList[moveCount++]=(Move(fromSq, toSq, MoveFlag::CPROMOTION_QUEEN));
                moveList[moveCount++]=(Move(fromSq, toSq, MoveFlag::CPROMOTION_ROOK));
                moveList[moveCount++]=(Move(fromSq, toSq, MoveFlag::CPROMOTION_BISHOP));
                moveList[moveCount++]=(Move(fromSq, toSq, MoveFlag::CPROMOTION_KNIGHT));
            } else {
                moveList[moveCount++]=(Move(fromSq, toSq, MoveFlag::CAPTURE));
            }
            popBit(attacks, toSq);
        }
        if (board.enPassantSq != -1) {
            uint64_t epBit = (1ULL << board.enPassantSq);
            if (pawnAttacks[color][fromSq] & epBit) {
                moveList[moveCount++]=(Move(fromSq, board.enPassantSq, MoveFlag::EN_PASSANT));
            }
        }
        popBit(tempPawns, fromSq);
    }
}
void GenerateMoves::generateKnightMoves(const Board& board, Move* moveList, Color color, int& moveCount){
    uint64_t empty = ~board.getOccupancy(BOTH);
    uint64_t enemies = board.getOccupancy(color == WHITE ? BLACK : WHITE);
    uint64_t knights = board.getPieceBB(color == WHITE ? W_KNIGHT : B_KNIGHT);
    uint64_t tempKnights = knights; 
    while(tempKnights){
    int fromSq = getLS1B(tempKnights);
    uint64_t attackBB = knightMoves[fromSq];
    uint64_t attacks = attackBB & enemies;
    while(attacks){
        int toSq = getLS1B(attacks);
        moveList[moveCount++]=(Move(fromSq,toSq,MoveFlag::CAPTURE));
        popBit(attacks,toSq);
    }
    uint64_t quietMoveKnight = attackBB & empty;
    while(quietMoveKnight){
        int toSq = getLS1B(quietMoveKnight);
        moveList[moveCount++]=(Move(fromSq,toSq,MoveFlag::QUIET));
        popBit(quietMoveKnight,toSq);
    }
    popBit(tempKnights,fromSq);
}
}
void GenerateMoves::generateKingMoves(const Board& board, Move* moveList, Color color, int& moveCount){
    uint64_t empty = ~board.getOccupancy(BOTH);
    uint64_t enemies = board.getOccupancy(color == WHITE ? BLACK : WHITE);
    uint64_t kingSquare = board.getPieceBB(color == WHITE ? W_KING : B_KING);
    uint64_t tempKing = kingSquare;
    while(tempKing){
    int fromSq = getLS1B(tempKing);
    uint64_t attackBB = kingMoves[fromSq];
    uint64_t attacks = attackBB & enemies;
    while(attacks){
        int toSq = getLS1B(attacks);
        moveList[moveCount++]=(Move(fromSq,toSq,MoveFlag::CAPTURE));
        popBit(attacks,toSq);
    }
    uint64_t quietMoveKing = attackBB & empty;
    while(quietMoveKing){
        int toSq = getLS1B(quietMoveKing);
        moveList[moveCount++]=(Move(fromSq,toSq,MoveFlag::QUIET));
        popBit(quietMoveKing,toSq);
    }
    popBit(tempKing,fromSq);
    }
    int rights = board.castlingRights;
    uint64_t occ = board.getOccupancy(BOTH);

    if (color == WHITE) {
        if (rights & WK) {
            if (!(occ & ((1ULL << f1) | (1ULL << g1)))) {
                if (!isSquareAttacked(board, e1, BLACK) && 
                    !isSquareAttacked(board, f1, BLACK) && 
                    !isSquareAttacked(board, g1, BLACK)) {
                    moveList[moveCount++]=(Move(e1, g1, MoveFlag::KING_CASTLE));
                }
            }
        }
        if(rights & WQ) {
            if(!(occ & ((1ULL << d1) | (1ULL << c1) | (1ULL << b1)))){
                if (!isSquareAttacked(board, e1, BLACK) && 
                    !isSquareAttacked(board, d1, BLACK) && 
                    !isSquareAttacked(board, c1, BLACK)) {
                    moveList[moveCount++]=(Move(e1, c1, MoveFlag::QUEEN_CASTLE));
                }
            }
        }
    }
    else{
        if (rights & BK) {
            if (!(occ & ((1ULL << f8) | (1ULL << g8)))) {
                if (!isSquareAttacked(board, e8, WHITE) && 
                    !isSquareAttacked(board, f8, WHITE) && 
                    !isSquareAttacked(board, g8, WHITE)) {
                    moveList[moveCount++]=(Move(e8, g8, MoveFlag::KING_CASTLE));
                }
            }
        }
        if(rights & BQ) {
            if(!(occ & ((1ULL << d8) | (1ULL << c8) | (1ULL << b8)))){
                if (!isSquareAttacked(board, e8, WHITE) && 
                    !isSquareAttacked(board, d8, WHITE) && 
                    !isSquareAttacked(board, c8, WHITE)) {
                    moveList[moveCount++]=(Move(e8, c8, MoveFlag::QUEEN_CASTLE));
                }
            }
        }
    }
}
void GenerateMoves::generateRookMoves(const Board& board, Move* moveList, Color color, int& moveCount){
    uint64_t occupancy = board.getOccupancy(BOTH);
    uint64_t enemies = board.getOccupancy(color == WHITE ? BLACK : WHITE);
    uint64_t friends = board.getOccupancy(color);

    uint64_t rookSquare = board.getPieceBB(color == WHITE ? W_ROOK : B_ROOK);
    uint64_t tempRook = rookSquare;
    while(tempRook){
        int fromSq = getLS1B(tempRook);
        uint64_t maskedOcc = occupancy & rookMagics[fromSq].mask;
        uint64_t magicIdx = (maskedOcc * rookMagics[fromSq].magic) >> rookMagics[fromSq].shift;
        uint64_t possibleRookMoves = rookMagics[fromSq].ptr[magicIdx] & ~friends;
        uint64_t quietMoves = possibleRookMoves & ~enemies;
        uint64_t attackMoves = possibleRookMoves & enemies;
        while(quietMoves){
            int toSq = getLS1B(quietMoves);
            moveList[moveCount++]=(Move(fromSq,toSq,MoveFlag::QUIET));
            popBit(quietMoves,toSq);
        }
        while(attackMoves){
            int toSq = getLS1B(attackMoves);
            moveList[moveCount++]=(Move(fromSq,toSq,MoveFlag::CAPTURE));
            popBit(attackMoves,toSq);
        }
        popBit(tempRook,fromSq);
    }
}
void GenerateMoves::generateBishopMoves(const Board& board, Move* moveList, Color color, int& moveCount){
    uint64_t occupancy = board.getOccupancy(BOTH);
    uint64_t enemies = board.getOccupancy(color == WHITE ? BLACK : WHITE);
    uint64_t friends = board.getOccupancy(color);

    uint64_t bishopSquare = board.getPieceBB(color == WHITE ? W_BISHOP : B_BISHOP);
    uint64_t tempBishop = bishopSquare;
    while(tempBishop){
        int fromSq = getLS1B(tempBishop);
        uint64_t maskedOcc = occupancy & bishopMagics[fromSq].mask;
        uint64_t magicIdx = (maskedOcc * bishopMagics[fromSq].magic) >> bishopMagics[fromSq].shift;
        uint64_t possibleBishopMoves = bishopMagics[fromSq].ptr[magicIdx] & ~friends;
        uint64_t quietMoves = possibleBishopMoves & ~enemies;
        uint64_t attackMoves = possibleBishopMoves & enemies;
        while(quietMoves){
            int toSq = getLS1B(quietMoves);
            moveList[moveCount++]=(Move(fromSq,toSq,MoveFlag::QUIET));
            popBit(quietMoves,toSq);
        }
        while(attackMoves){
            int toSq = getLS1B(attackMoves);
            moveList[moveCount++]=(Move(fromSq,toSq,MoveFlag::CAPTURE));
            popBit(attackMoves,toSq);
        }
        popBit(tempBishop,fromSq);
    }
}
void GenerateMoves::generateQueenMoves(const Board& board, Move* moveList, Color color, int& moveCount) {

    uint64_t occupancy = board.getOccupancy(BOTH);
    uint64_t enemies = board.getOccupancy(color == WHITE ? BLACK : WHITE);
    uint64_t friends = board.getOccupancy(color);

    uint64_t queens = board.getPieceBB(color == WHITE ? W_QUEEN : B_QUEEN);
    
    while (queens) {
        int fromSq = getLS1B(queens);
        uint64_t rMasked = occupancy & rookMagics[fromSq].mask;
        uint64_t rAttacks = rookMagics[fromSq].ptr[(rMasked * rookMagics[fromSq].magic) >> rookMagics[fromSq].shift];
        uint64_t bMasked = occupancy & bishopMagics[fromSq].mask;
        uint64_t bAttacks = bishopMagics[fromSq].ptr[(bMasked * bishopMagics[fromSq].magic) >> bishopMagics[fromSq].shift];
        uint64_t possibleMoves = (rAttacks | bAttacks) & ~friends;
        uint64_t quietMoves = possibleMoves & ~enemies;
        uint64_t attackMoves = possibleMoves & enemies;
        while (quietMoves) {
            int toSq = getLS1B(quietMoves);
            moveList[moveCount++]=(Move(fromSq, toSq, MoveFlag::QUIET));
            popBit(quietMoves, toSq);
        }
        while (attackMoves) {
            int toSq = getLS1B(attackMoves);
            moveList[moveCount++]=(Move(fromSq, toSq, MoveFlag::CAPTURE));
            popBit(attackMoves, toSq);
        }
        popBit(queens, fromSq);
    }
}

bool GenerateMoves::isSquareAttacked(const Board& board, int sq, Color attackerColor) {

    uint64_t occupancy = board.getOccupancy(BOTH);
    uint64_t enemyKnights = board.getPieceBB(attackerColor == WHITE ? W_KNIGHT : B_KNIGHT);
    if (knightMoves[sq] & enemyKnights) return true;

    Color squareColor = (attackerColor == WHITE) ? BLACK : WHITE;
    uint64_t enemyPawns = board.getPieceBB(attackerColor == WHITE ? W_PAWN : B_PAWN);
    if (pawnAttacks[squareColor][sq] & enemyPawns) return true;

    uint64_t rMasked = occupancy & rookMagics[sq].mask;
    uint64_t rAttacks = rookMagics[sq].ptr[(rMasked * rookMagics[sq].magic) >> rookMagics[sq].shift];
    uint64_t rooksQueens = board.getPieceBB(attackerColor == WHITE ? W_ROOK : B_ROOK) | 
                           board.getPieceBB(attackerColor == WHITE ? W_QUEEN : B_QUEEN);
    if (rAttacks & rooksQueens) return true;
    uint64_t bMasked = occupancy & bishopMagics[sq].mask;
    uint64_t bAttacks = bishopMagics[sq].ptr[(bMasked * bishopMagics[sq].magic) >> bishopMagics[sq].shift];
    uint64_t bishopsQueens = board.getPieceBB(attackerColor == WHITE ? W_BISHOP : B_BISHOP) | 
                             board.getPieceBB(attackerColor == WHITE ? W_QUEEN : B_QUEEN);
    if (bAttacks & bishopsQueens) return true;
    uint64_t enemyKing = board.getPieceBB(attackerColor == WHITE ? W_KING : B_KING);
    if (kingMoves[sq] & enemyKing) return true;

    return false;
}

int GenerateMoves::findLVA(const Board& board, int sq, Color attackerColor, uint64_t occupancy){
    Color squareColor = (attackerColor == WHITE) ? BLACK : WHITE;
    uint64_t attackerPawns = board.getPieceBB(attackerColor == WHITE ? W_PAWN : B_PAWN);
    uint64_t bbToReturn;
    bbToReturn = pawnAttacks[squareColor][sq] & attackerPawns;
    if (bbToReturn) return getLS1B(bbToReturn);

    uint64_t attackerKnights = board.getPieceBB(attackerColor == WHITE ? W_KNIGHT : B_KNIGHT);
    bbToReturn = knightMoves[sq] & attackerKnights;
    if (bbToReturn) return getLS1B(bbToReturn);

    uint64_t rookMagicBB = getRookMagicBB(sq,occupancy);
    uint64_t bishopMagicBB = getBishopMagicBB(sq,occupancy);
    uint64_t queensBB = rookMagicBB | bishopMagicBB;
    uint64_t bishops = board.getPieceBB(attackerColor == WHITE ? W_BISHOP : B_BISHOP);
    bbToReturn = bishops & bishopMagicBB;
    if (bbToReturn) return getLS1B(bbToReturn);
    uint64_t rooks = board.getPieceBB(attackerColor == WHITE ? W_ROOK : B_ROOK);    
    bbToReturn = rooks & rookMagicBB;  
    if (bbToReturn) return getLS1B(bbToReturn);
    uint64_t queens = board.getPieceBB(attackerColor == WHITE ? W_QUEEN : B_QUEEN);
    bbToReturn = queens & queensBB;
    if (bbToReturn) return getLS1B(bbToReturn);

    uint64_t attackerKing = board.getPieceBB(attackerColor == WHITE ? W_KING : B_KING);
    bbToReturn = kingMoves[sq] & attackerKing;
    if (bbToReturn) return getLS1B(bbToReturn);

    return -1;
}




std::string GenerateMoves::squareToString(int sq) {
    if (sq < 0 || sq > 63) return "??";
    char file = 'a' + (sq % 8);
    char rank = '1' + (sq / 8);
    return {file, rank};
}

void GenerateMoves::printMoveList(const std::vector<Move>& moveList) {

    std::cout << "--- Move List (" << moveList.size() << " moves) ---" << std::endl;
    
    for (const auto& move : moveList) {
        int from = move.from;
        int to = move.to;
        MoveFlag flag = move.flag;

        std::cout << squareToString(from) << squareToString(to);

        // Print human-readable flag info
        switch (flag) {
            case MoveFlag::QUIET:            std::cout << " (Quiet)"; break;
            case MoveFlag::CAPTURE:          std::cout << " (Capture)"; break;
            case MoveFlag::DOUBLE_PAWN_PUSH: std::cout << " (Double Push)"; break;
            case MoveFlag::KING_CASTLE:      std::cout << " (O-O)"; break;
            case MoveFlag::QUEEN_CASTLE:     std::cout << " (O-O-O)"; break;
            case MoveFlag::EN_PASSANT:       std::cout << " (EP Capture)"; break;
            case MoveFlag::PROMOTION_QUEEN:  std::cout << "q (Promotion)"; break;
            case MoveFlag::CPROMOTION_QUEEN: std::cout << "q (Capture Promotion)"; break;
            case MoveFlag::PROMOTION_ROOK:  std::cout << "r (Promotion)"; break;
            case MoveFlag::CPROMOTION_ROOK: std::cout << "r (Capture Promotion)"; break;
            case MoveFlag::PROMOTION_BISHOP:  std::cout << "b (Promotion)"; break;
            case MoveFlag::CPROMOTION_BISHOP: std::cout << "b (Capture Promotion)"; break;
            case MoveFlag::PROMOTION_KNIGHT:  std::cout << "n (Promotion)"; break;
            case MoveFlag::CPROMOTION_KNIGHT: std::cout << "n (Capture Promotion)"; break;
            default: break;
        }
        std::cout << std::endl;
    }
}

void GenerateMoves::generatePawnCaptures(const Board& board, Move* moveList, Color color, int& moveCount) {
    uint64_t pawns = board.getPieceBB(color == WHITE ? W_PAWN : B_PAWN);
    uint64_t enemies = board.getOccupancy(color == WHITE ? BLACK : WHITE);
    
    while (pawns) {
        int fromSq = getLS1B(pawns);
        uint64_t attacks = pawnAttacks[color][fromSq] & enemies;
        
        while (attacks) {
            int toSq = getLS1B(attacks);
            bool isPromotion = (color == WHITE) ? (toSq >= 56) : (toSq <= 7);
            
            if (isPromotion) {
                moveList[moveCount++] = Move(fromSq, toSq, MoveFlag::CPROMOTION_QUEEN);
                moveList[moveCount++] = Move(fromSq, toSq, MoveFlag::CPROMOTION_ROOK);
                moveList[moveCount++] = Move(fromSq, toSq, MoveFlag::CPROMOTION_BISHOP);
                moveList[moveCount++] = Move(fromSq, toSq, MoveFlag::CPROMOTION_KNIGHT);
            } else {
                moveList[moveCount++] = Move(fromSq, toSq, MoveFlag::CAPTURE);
            }
            popBit(attacks, toSq);
        }
        
        if (board.enPassantSq != -1) {
            uint64_t epBit = (1ULL << board.enPassantSq);
            if (pawnAttacks[color][fromSq] & epBit) {
                moveList[moveCount++] = Move(fromSq, board.enPassantSq, MoveFlag::EN_PASSANT);
            }
        }
        popBit(pawns, fromSq);
    }
}
void GenerateMoves::generateKnightCaptures(const Board& board, Move* moveList, Color color, int& moveCount) {
    uint64_t enemies = board.getOccupancy(color == WHITE ? BLACK : WHITE);
    uint64_t knights = board.getPieceBB(color == WHITE ? W_KNIGHT : B_KNIGHT);
    
    while(knights) {
        int fromSq = getLS1B(knights);
        uint64_t attacks = knightMoves[fromSq] & enemies;
        
        while(attacks) {
            int toSq = getLS1B(attacks);
            moveList[moveCount++] = Move(fromSq, toSq, MoveFlag::CAPTURE);
            popBit(attacks, toSq);
        }
        popBit(knights, fromSq);
    }
}
void GenerateMoves::generateKingCaptures(const Board& board, Move* moveList, Color color, int& moveCount) {
    uint64_t enemies = board.getOccupancy(color == WHITE ? BLACK : WHITE);
    uint64_t kings = board.getPieceBB(color == WHITE ? W_KING : B_KING);
    
    while(kings) {
        int fromSq = getLS1B(kings);
        uint64_t attacks = kingMoves[fromSq] & enemies;
        
        while(attacks) {
            int toSq = getLS1B(attacks);
            moveList[moveCount++] = Move(fromSq, toSq, MoveFlag::CAPTURE);
            popBit(attacks, toSq);
        }
        popBit(kings, fromSq);
    }
}
void GenerateMoves::generateRookCaptures(const Board& board, Move* moveList, Color color, int& moveCount) {
    uint64_t occupancy = board.getOccupancy(BOTH);
    uint64_t enemies = board.getOccupancy(color == WHITE ? BLACK : WHITE);

    uint64_t rooks = board.getPieceBB(color == WHITE ? W_ROOK : B_ROOK);
    
    while(rooks) {
        int fromSq = getLS1B(rooks);
        uint64_t maskedOcc = occupancy & rookMagics[fromSq].mask;
        uint64_t magicIdx = (maskedOcc * rookMagics[fromSq].magic) >> rookMagics[fromSq].shift;
        
        uint64_t attackMoves = rookMagics[fromSq].ptr[magicIdx] & enemies;
        
        while(attackMoves) {
            int toSq = getLS1B(attackMoves);
            moveList[moveCount++] = Move(fromSq, toSq, MoveFlag::CAPTURE);
            popBit(attackMoves, toSq);
        }
        popBit(rooks, fromSq);
    }
}
void GenerateMoves::generateBishopCaptures(const Board& board, Move* moveList, Color color, int& moveCount) {
    uint64_t occupancy = board.getOccupancy(BOTH);
    uint64_t enemies = board.getOccupancy(color == WHITE ? BLACK : WHITE);

    uint64_t bishops = board.getPieceBB(color == WHITE ? W_BISHOP : B_BISHOP);
    
    while(bishops) {
        int fromSq = getLS1B(bishops);
        uint64_t maskedOcc = occupancy & bishopMagics[fromSq].mask;
        uint64_t magicIdx = (maskedOcc * bishopMagics[fromSq].magic) >> bishopMagics[fromSq].shift;
        
        uint64_t attackMoves = bishopMagics[fromSq].ptr[magicIdx] & enemies;
        
        while(attackMoves) {
            int toSq = getLS1B(attackMoves);
            moveList[moveCount++] = Move(fromSq, toSq, MoveFlag::CAPTURE);
            popBit(attackMoves, toSq);
        }
        popBit(bishops, fromSq);
    }
}
void GenerateMoves::generateQueenCaptures(const Board& board, Move* moveList, Color color, int& moveCount) {
    uint64_t occupancy = board.getOccupancy(BOTH);
    uint64_t enemies = board.getOccupancy(color == WHITE ? BLACK : WHITE);

    uint64_t queens = board.getPieceBB(color == WHITE ? W_QUEEN : B_QUEEN);
    
    while (queens) {
        int fromSq = getLS1B(queens);
        
        uint64_t rMasked = occupancy & rookMagics[fromSq].mask;
        uint64_t rAttacks = rookMagics[fromSq].ptr[(rMasked * rookMagics[fromSq].magic) >> rookMagics[fromSq].shift];
        
        uint64_t bMasked = occupancy & bishopMagics[fromSq].mask;
        uint64_t bAttacks = bishopMagics[fromSq].ptr[(bMasked * bishopMagics[fromSq].magic) >> bishopMagics[fromSq].shift];

        uint64_t attackMoves = (rAttacks | bAttacks) & enemies;
        
        while (attackMoves) {
            int toSq = getLS1B(attackMoves);
            moveList[moveCount++] = Move(fromSq, toSq, MoveFlag::CAPTURE);
            popBit(attackMoves, toSq);
        }
        popBit(queens, fromSq);
    }
}


uint64_t GenerateMoves::getRookMagicBB(int sq, uint64_t occupancy){
    Magic RookMagics = getRookMagic(sq);
    return RookMagics.ptr[(occupancy & RookMagics.mask) * RookMagics.magic >> RookMagics.shift];
}
uint64_t GenerateMoves::getBishopMagicBB(int sq, uint64_t occupancy){
    Magic BishopMagics = getBishopMagic(sq);
    return BishopMagics.ptr[(occupancy & BishopMagics.mask) * BishopMagics.magic >> BishopMagics.shift];
}



void GenerateMoves::generateCheckKnightMoves(const Board& board, Move* quietCheckMoveList, Color color, int& moveCount){
    uint64_t empty = ~board.getOccupancy(BOTH);
    uint64_t knights = board.getPieceBB(color == WHITE ? W_KNIGHT : B_KNIGHT);
    
    Color enemyColor = (color == WHITE) ? BLACK : WHITE;
    int kingSq = board.getKingSquare(enemyColor);
    
    uint64_t kingKnightAttacks = knightMoves[kingSq];

    uint64_t tempKnights = knights; 
    while(tempKnights){
        int fromSq = getLS1B(tempKnights);
        uint64_t attackBB = knightMoves[fromSq];
        uint64_t quietMoveKnight = attackBB & empty;
        
        while(quietMoveKnight){
            int toSq = getLS1B(quietMoveKnight);

            if ((1ULL << toSq) & kingKnightAttacks) {
                quietCheckMoveList[moveCount++] = Move(fromSq, toSq, MoveFlag::QUIET);
            }
            
            popBit(quietMoveKnight, toSq);
        }
        popBit(tempKnights, fromSq);
    }
}
void GenerateMoves::generateCheckBishopMoves(const Board& board, Move* quietCheckMoveList, Color color, int& moveCount){
    uint64_t occupancy = board.getOccupancy(BOTH);
    uint64_t empty = ~occupancy;
    uint64_t bishops = board.getPieceBB(color == WHITE ? W_BISHOP : B_BISHOP);
    
    Color enemyColor = (color == WHITE) ? BLACK : WHITE;
    int kingSq = board.getKingSquare(enemyColor);

    uint64_t tempBishops = bishops;
    while(tempBishops){
        int fromSq = getLS1B(tempBishops);
        
        uint64_t quietMoves = getBishopMagicBB(fromSq, occupancy) & empty;
        
        uint64_t kingBishopAttacks = getBishopMagicBB(kingSq, occupancy);
        
        uint64_t directChecks = quietMoves & kingBishopAttacks;
        
        while(directChecks){
            int toSq = getLS1B(directChecks);
            quietCheckMoveList[moveCount++] = Move(fromSq, toSq, MoveFlag::QUIET);
            popBit(directChecks, toSq);
        }
        
        popBit(tempBishops, fromSq);
    }
}
void GenerateMoves::generateCheckRookMoves(const Board& board, Move* quietCheckMoveList, Color color, int& moveCount){
    uint64_t occupancy = board.getOccupancy(BOTH);
    uint64_t empty = ~occupancy;
    uint64_t rooks = board.getPieceBB(color == WHITE ? W_ROOK : B_ROOK);
    
    Color enemyColor = (color == WHITE) ? BLACK : WHITE;
    int kingSq = board.getKingSquare(enemyColor);

    uint64_t tempRooks = rooks;
    while(tempRooks){
        int fromSq = getLS1B(tempRooks);
        
        uint64_t quietMoves = getRookMagicBB(fromSq, occupancy) & empty;
        
        uint64_t kingRookAttacks = getRookMagicBB(kingSq, occupancy);
        
        uint64_t directChecks = quietMoves & kingRookAttacks;
        
        while(directChecks){
            int toSq = getLS1B(directChecks);
            quietCheckMoveList[moveCount++] = Move(fromSq, toSq, MoveFlag::QUIET);
            popBit(directChecks, toSq);
        }
        
        popBit(tempRooks, fromSq);
    }
}
void GenerateMoves::generateCheckQueenMoves(const Board& board, Move* quietCheckMoveList, Color color, int& moveCount){
    uint64_t occupancy = board.getOccupancy(BOTH);
    uint64_t empty = ~occupancy;
    uint64_t queens = board.getPieceBB(color == WHITE ? W_QUEEN : B_QUEEN);
    
    Color enemyColor = (color == WHITE) ? BLACK : WHITE;
    int kingSq = board.getKingSquare(enemyColor);
    uint64_t kingRookAttacks = getRookMagicBB(kingSq, occupancy);
    uint64_t kingBishopAttacks = getBishopMagicBB(kingSq, occupancy);
    uint64_t kingQueenAttacks = kingRookAttacks | kingBishopAttacks;

    uint64_t tempQueens = queens;
    while(tempQueens){
        int fromSq = getLS1B(tempQueens);
        
        uint64_t quietMovesRook = getRookMagicBB(fromSq, occupancy) & empty;
        uint64_t quietMovesBishop = getBishopMagicBB(fromSq, occupancy) & empty;
        uint64_t quietMoves = quietMovesRook | quietMovesBishop;
        uint64_t directChecks = quietMoves & kingQueenAttacks;
        
        while(directChecks){
            int toSq = getLS1B(directChecks);
            quietCheckMoveList[moveCount++] = Move(fromSq, toSq, MoveFlag::QUIET);
            popBit(directChecks, toSq);
        }
        
        popBit(tempQueens, fromSq);
    }
}
void GenerateMoves::generateCheckPawnMoves(const Board& board, Move* quietCheckMoveList, Color color, int& moveCount) {
    uint64_t pawns = board.getPieceBB(color == WHITE ? W_PAWN : B_PAWN);
    uint64_t empty = ~board.getOccupancy(BOTH);
    
    Color enemyColor = (color == WHITE) ? BLACK : WHITE;
    int kingSq = board.getKingSquare(enemyColor);
    


    uint64_t pawnCheckSquares = pawnAttacks[enemyColor][kingSq]; 

    uint64_t promotionRank = (color == WHITE) ? RANK_8 : RANK_1;
    uint64_t dblPushRank = (color == WHITE) ? RANK_4 : RANK_5;
    int shift = (color == WHITE) ? 8 : -8;

    uint64_t singlePushes = (color == WHITE) ? (pawns << 8) & empty : (pawns >> 8) & empty;
    uint64_t doublePushes = (color == WHITE) ? (singlePushes << 8) & empty & dblPushRank 
                                             : (singlePushes >> 8) & empty & dblPushRank;

    uint64_t normalCheckingPushes = (singlePushes & ~promotionRank) & pawnCheckSquares;
    uint64_t checkingDoublePushes = doublePushes & pawnCheckSquares;

    while (normalCheckingPushes) {
        int toSq = getLS1B(normalCheckingPushes);
        quietCheckMoveList[moveCount++] = Move(toSq - shift, toSq, MoveFlag::QUIET);
        popBit(normalCheckingPushes, toSq);
    }
    
    while (checkingDoublePushes) {
        int toSq = getLS1B(checkingDoublePushes);
        quietCheckMoveList[moveCount++] = Move(toSq - (shift * 2), toSq, MoveFlag::DOUBLE_PAWN_PUSH);
        popBit(checkingDoublePushes, toSq);
    }
    uint64_t promotionPushes = singlePushes & promotionRank;
    
    uint64_t occupancy = board.getOccupancy(BOTH);
    uint64_t kingQueenAttacks = getRookMagicBB(kingSq, occupancy) | getBishopMagicBB(kingSq, occupancy);
    uint64_t kingKnightAttacks = knightMoves[kingSq];

    while (promotionPushes) {
        int toSq = getLS1B(promotionPushes);
        int fromSq = toSq - shift;

        if ((1ULL << toSq) & kingQueenAttacks) {
            quietCheckMoveList[moveCount++] = Move(fromSq, toSq, MoveFlag::PROMOTION_QUEEN);
            if ((1ULL << toSq) & getRookMagicBB(kingSq, occupancy)) {
                quietCheckMoveList[moveCount++] = Move(fromSq, toSq, MoveFlag::PROMOTION_ROOK);
            }
            if ((1ULL << toSq) & getBishopMagicBB(kingSq, occupancy)) {
                quietCheckMoveList[moveCount++] = Move(fromSq, toSq, MoveFlag::PROMOTION_BISHOP);
            }
        }
        
        if ((1ULL << toSq) & kingKnightAttacks) {
            quietCheckMoveList[moveCount++] = Move(fromSq, toSq, MoveFlag::PROMOTION_KNIGHT);
        }
        
        popBit(promotionPushes, toSq);
    }
}