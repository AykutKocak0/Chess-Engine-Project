#include <vector>
#include "Types.h" 

enum HashFlag { HASH_EXACT, HASH_ALPHA, HASH_BETA };

struct TTEntry {
    uint64_t key;  
    int score;     
    int depth;     
    Move bestMove;
    uint8_t flag;
    uint8_t age;
};

struct PawnEntry {
    uint64_t key;
    int mgScore;
    int egScore;
};

class TranspositionTable {
public:
    TranspositionTable(size_t mbSize, bool isPawnTable = false);
    // Reallocates the table with a new size in MB and clears it. Must not be
    // called while a search is running.
    void resize(size_t mbSize);
    void clear();
    void newSearch() { generation++; }
    void store(uint64_t key, int depth, int score, HashFlag flag, Move bestMove);
    void store(uint64_t key, int mgScore, int egScore);
    TTEntry* probe(uint64_t key);
    PawnEntry* pawnProbe(uint64_t key);

private:
    std::vector<TTEntry> table;
    std::vector<PawnEntry> pawnTable;
    size_t numEntries;
    bool isPawn;
    uint8_t generation = 0;
};
extern TranspositionTable TT;
extern TranspositionTable PTT;