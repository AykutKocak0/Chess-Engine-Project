#pragma once
#include <cstdint>
#include <istream>
#include <string>
#include <vector>

// Texel tuning of the evaluation weights in EvalParams.
//
// Reads EPD lines written by datagen ("<position> c9 "1-0"; ce 35;"), then
// changes one weight at a time by +-1 and keeps the change when the mean squared
// error between the game results and sigmoid(K * eval) goes down.
struct TuneConfig {
    std::vector<std::string> files;
    int threads = 0;            // 0 = all hardware threads
    double lambda = 1.0;        // target = lambda * result + (1 - lambda) * sigmoid(search score)
    size_t maxPositions = 0;    // 0 = use every position
    int iterations = 50;        // passes over all weights
    std::string out = "tuned";  // output file prefix, see runTuner
    uint64_t seed = 1;          // for shuffling when maxPositions is used
    // Share of each file (its last part, so whole games) kept out of tuning and
    // used only to measure the error on positions the tuner has not seen.
    double validFraction = 0.1;
    // Stop after this many iterations in a row without a new best validation error.
    int patience = 3;
};

// "tune [threads=N] [lambda=X] [max=N] [iters=N] [out=NAME] [seed=N] [valid=X] [patience=N] files..."
// Takes the arguments one by one so that file paths may contain spaces.
TuneConfig parseTuneArgs(const std::vector<std::string>& args);
void runTuner(const TuneConfig& config);
