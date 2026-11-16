#pragma once
#include <cstdint>
#include <string>

class Search;

// Self-play data generation for evaluation tuning (Texel tuning, later NNUE).
//
// Each game starts from a few random legal moves, then the engine plays itself
// with a fixed node budget per move. Quiet positions from the game are sampled
// and written as EPD lines labeled with the game result:
//
//   <board> <side> <castling> <ep> c9 "1-0"; ce 35;
//
// c9 is the result from White's point of view, ce is the search score in
// centipawns from the side to move's point of view (standard EPD meaning).
struct DatagenConfig {
    int games = 1000;
    int nodes = 5000;           // search nodes per move
    std::string outFile = "data.epd";
    uint64_t seed = 1;          // use a different seed for each parallel process
    int positionsPerGame = 16;  // quiet positions sampled from each game
};

void runDatagen(const DatagenConfig& config, Search& search);
