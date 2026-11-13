#include "TransPositionTable.h"
#include <algorithm>
TranspositionTable TT(64);
TranspositionTable PTT(16, true);
TranspositionTable::TranspositionTable(size_t mbSize, bool isPawnTable) {
    // Each instance only allocates the table it is actually used for.
    if (isPawnTable) {
        numEntries = (mbSize * 1024 * 1024) / sizeof(PawnEntry);
        pawnTable.resize(numEntries);
    } else {
        numEntries = (mbSize * 1024 * 1024) / sizeof(TTEntry);
        table.resize(numEntries);
    }
    clear();
}

void TranspositionTable::clear() {
    generation = 0;
    for (auto& entry : table) {
        entry.key = 0;
        entry.depth = -1;
        entry.bestMove = Move();
        entry.flag = 0;
        entry.age = 0;
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

    // Entries left over from an earlier search are always replaceable.
    if (entry->key == 0 || entry->age != generation || depth >= entry->depth) {
        entry->key = key;
        entry->score = score;
        entry->depth = depth;
        entry->flag = (uint8_t)flag;
        entry->age = generation;
        entry->bestMove = bestMove;
    }
}
void TranspositionTable::store(uint64_t key, int mgScore, int egScore){
    size_t index = key % numEntries;
    PawnEntry* entry = &pawnTable[index];

    entry->key = key;
    entry->mgScore = mgScore;
    entry->egScore = egScore;
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
