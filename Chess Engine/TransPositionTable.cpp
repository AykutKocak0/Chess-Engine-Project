#include "TransPositionTable.h"
#include <algorithm>
TranspositionTable TT(64);
TranspositionTable PTT(64);
TranspositionTable::TranspositionTable(size_t mbSize) {
    numEntries = (mbSize * 1024 * 1024) / sizeof(TTEntry);
    pawnTable.resize(numEntries);
    table.resize(numEntries);
    clear();
}

void TranspositionTable::clear() {
    for (auto& entry : table) {
        entry.key = 0;
        entry.depth = -1; 
        entry.bestMove = Move(); 
        entry.flag = 0;
    }

    
    for (auto& entry : pawnTable) {
    entry.key = 0;
    entry.mgScore = 0; 
    entry.egScore = 0;
    }
}

void TranspositionTable::store(uint64_t key, int depth, int score, HashFlag flag, Move bestMove) {
    size_t index = key % numEntries;
    TTEntry* entry = &table[index];

    if (entry->key == 0 || depth >= entry->depth) {
        entry->key = key;
        entry->score = score;
        entry->depth = depth;
        entry->flag = (uint8_t)flag;
        entry->bestMove = bestMove;
    }
}
void TranspositionTable::store(uint64_t key, int mgScore, int egScore){
    size_t index = key % numEntries;
    PawnEntry* entry = &pawnTable[index];

    if (entry->key == 0) {
        entry->key = key;
        entry->mgScore = mgScore;
        entry->egScore = egScore;
    }

}

TTEntry* TranspositionTable::probe(uint64_t key) {
    size_t index = key % numEntries;
    TTEntry* entry = &table[index];

    if (entry->key == key) {
        return entry;
    }

    return nullptr; 
}

PawnEntry* TranspositionTable::pawnProbe(uint64_t key){
    size_t index = key % numEntries;
    PawnEntry* entry = &pawnTable[index];

    if (entry->key == key) {
        return entry;
    }

    return nullptr; 
}