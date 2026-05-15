#pragma once
#include <stdint.h>
#ifdef _MSC_VER
#include <intrin.h>
#define __builtin_popcountll __popcnt64
static inline int __builtin_ctzll(unsigned __int64 mask) {
    unsigned long where;
    if (_BitScanForward64(&where, mask)) {
        return (int)where;
    }
    return 64; 
}
static inline int __builtin_clzll(unsigned __int64 mask) {
    if (mask == 0) return 64;
    unsigned long where;
    _BitScanReverse64(&where, mask);
    return 64 - 1 - (int)where;
}

#endif

inline int getLS1B(uint64_t bb) {
    return __builtin_ctzll(bb);
}
inline void popBit(uint64_t &bb, int sq) {
    bb &= ~(1ULL << sq);
}
inline void setBit(uint64_t &bb, int sq) {
    bb |= (1ULL << sq);
}
inline uint64_t getBit(uint64_t bb, int sq) {
    return bb & (1ULL << sq);
}
enum PieceType {
    W_PAWN, W_KNIGHT, W_BISHOP, W_ROOK, W_QUEEN, W_KING,
    B_PAWN, B_KNIGHT, B_BISHOP, B_ROOK, B_QUEEN, B_KING,
    EMPTY
};
enum Color { 
    WHITE, 
    BLACK, 
    BOTH 
};
enum CastleRightCheck {
    WK = 0b1000, 
    WQ = 0b0100, 
    BK = 0b0010, 
    BQ = 0b0001
};
enum PieceValue {
    VAL_PAWN_MG   = 100,
    VAL_KNIGHT_MG = 320,
    VAL_BISHOP_MG = 330,
    VAL_ROOK_MG   = 500,
    VAL_QUEEN_MG  = 900,
    VAL_PAWN_EG   = 125,
    VAL_KNIGHT_EG = 285,
    VAL_BISHOP_EG = 350,
    VAL_ROOK_EG   = 600,
    VAL_QUEEN_EG  = 1000,
    VAL_KING      = 20000 
};
const int PIECE_VALUES[14] = {
    100, 320, 330, 500, 900, 20000,  
    100, 320, 330, 500, 900, 20000,  
    0, 0                             
};
enum class MoveFlag {
    QUIET = 0,
    DOUBLE_PAWN_PUSH,
    KING_CASTLE,
    QUEEN_CASTLE,
    CAPTURE,
    EN_PASSANT,
    PROMOTION_QUEEN,
    PROMOTION_KNIGHT,
    PROMOTION_ROOK,
    PROMOTION_BISHOP,
    CPROMOTION_QUEEN,
    CPROMOTION_KNIGHT,
    CPROMOTION_ROOK,
    CPROMOTION_BISHOP,
    
};
enum Square : int {
  a1, b1, c1, d1, e1, f1, g1, h1,
  a2, b2, c2, d2, e2, f2, g2, h2,
  a3, b3, c3, d3, e3, f3, g3, h3,
  a4, b4, c4, d4, e4, f4, g4, h4,
  a5, b5, c5, d5, e5, f5, g5, h5,
  a6, b6, c6, d6, e6, f6, g6, h6,
  a7, b7, c7, d7, e7, f7, g7, h7,
  a8, b8, c8, d8, e8, f8, g8, h8,
  none = 64
};
struct Move {
    int from;
    int to;
    MoveFlag flag;
    int score;

    

    bool isCapture() const {
        return (flag == MoveFlag::CAPTURE ||  
                flag == MoveFlag::EN_PASSANT);
    }
    bool isPromotion() const {
        return isQuietPromotion() || isCapturePromotion();
    }

    bool isCapturePromotion() const {
        return (flag == MoveFlag::CPROMOTION_QUEEN || flag == MoveFlag::CPROMOTION_ROOK || 
                flag == MoveFlag::CPROMOTION_BISHOP || flag == MoveFlag::CPROMOTION_KNIGHT);
    }

    bool isQuietPromotion() const {
        return (flag == MoveFlag::PROMOTION_QUEEN || flag == MoveFlag::PROMOTION_ROOK || 
                flag == MoveFlag::PROMOTION_BISHOP || flag == MoveFlag::PROMOTION_KNIGHT);
    }
    bool isCastle() const {
        return (flag == MoveFlag::KING_CASTLE || flag == MoveFlag::QUEEN_CASTLE);
    }
    Move(int f, int t, MoveFlag fl = MoveFlag::QUIET) 
        : from(f), to(t), flag(fl),score(-1) {}

    Move() : from(0), to(0), flag(MoveFlag::QUIET), score(-1) {}

    bool operator==(const Move& other) const {
    return (from == other.from && to == other.to && flag == other.flag);
}

    bool operator!=(const Move& other) const {
        return !(*this == other);
    }
};
struct GameState {
    int castlingRights;  
    int enPassantSq;    
    int capturedPiece;    
    int fiftyMoveClock;   
    int oldFiftyMoveClock;
    int startHistoryCount;
};
struct Magic {
    uint64_t mask;       
    uint64_t magic;     
    uint64_t* ptr;       
    int shift;         
};
enum PiecePhase {
    PawnPhase = 0,
    KnightPhase = 1,
    BishopPhase = 1,
    RookPhase = 2,
    QueenPhase = 4
};


