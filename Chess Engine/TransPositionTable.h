#include <vector>
#include "Types.h" 

enum HashFlag { HASH_EXACT, HASH_ALPHA, HASH_BETA };

struct TTEntry {
    uint64_t key;  
    int score;     
    int depth;     
    Move bestMove;
    uint8_t flag;   
};

struct PawnEntry {
    uint64_t key;
    int mgScore;
    int egScore;
};

class TranspositionTable {
public:
    TranspositionTable(size_t mbSize);
    void clear();
    void store(uint64_t key, int score, int depth, HashFlag flag, Move bestMove);
    void store(uint64_t key, int mgScore, int egScore);
    TTEntry* probe(uint64_t key);
    PawnEntry* pawnProbe(uint64_t key);

private:
    std::vector<TTEntry> table;
    std::vector<PawnEntry> pawnTable;
    size_t numEntries;
};
extern TranspositionTable TT;
extern TranspositionTable PTT;