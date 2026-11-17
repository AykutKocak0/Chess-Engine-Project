#include "Tuner.h"
#include "Board.h"
#include "EngineEval.h"
#include "EvalParams.h"
#include "GenerateMoves.h"
#include "TransPositionTable.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <thread>

namespace {

// One training position, stored compactly (a full Board is ~30 KB because of
// its history stacks; this is ~120 bytes).
struct Entry {
    uint64_t pieces[12];
    float result;       // game result from White's view: 1, 0.5 or 0
    float target;       // what the evaluation is fitted to (see TuneConfig::lambda)
    int16_t search;     // datagen search score, White's view
    uint8_t stm;
    uint8_t castling;
    int8_t ep;
};

using Clock = std::chrono::steady_clock;

double seconds(Clock::time_point since) {
    return std::chrono::duration<double>(Clock::now() - since).count();
}

// Win probability for a White-view score.
inline double sigmoid(double K, double score) {
    return 1.0 / (1.0 + std::exp(-K * score * (std::log(10.0) / 400.0)));
}

// Parses '<4 FEN fields> c9 "<result>"; ce <score>;'. Rejects incomplete lines
// (a file that is still being written can end in the middle of one).
bool parseLine(const std::string& line, Board& board, Entry& e) {
    size_t c9 = line.find(" c9 \"");
    if (c9 == std::string::npos) return false;
    size_t q1 = c9 + 5;
    size_t q2 = line.find('"', q1);
    if (q2 == std::string::npos) return false;
    std::string result = line.substr(q1, q2 - q1);
    if (result == "1-0") e.result = 1.0f;
    else if (result == "0-1") e.result = 0.0f;
    else if (result == "1/2-1/2") e.result = 0.5f;
    else return false;

    size_t ce = line.find("ce ", q2);
    if (ce == std::string::npos || line.find(';', ce) == std::string::npos) return false;
    int score = std::atoi(line.c_str() + ce + 3);

    board.parseFEN(line.substr(0, c9));
    if (__builtin_popcountll(board.getPieceBB(W_KING)) != 1 ||
        __builtin_popcountll(board.getPieceBB(B_KING)) != 1) {
        return false;
    }
    for (int i = 0; i < 12; i++) e.pieces[i] = board.getPieceBB((PieceType)i);
    e.stm = (uint8_t)board.sideToMove;
    e.castling = (uint8_t)board.castlingRights;
    e.ep = (int8_t)board.enPassantSq;
    score = std::max(-30000, std::min(30000, score));
    e.search = (int16_t)(board.sideToMove == WHITE ? score : -score);
    e.target = e.result;
    return true;
}

class Tuner {
public:
    std::vector<Entry> train;   // the weights are fitted to these
    std::vector<Entry> valid;   // only measured, never fitted to

    Tuner(const TuneConfig& config) : config(config) {
        threads = config.threads > 0 ? config.threads : (int)std::thread::hardware_concurrency();
        if (threads < 1) threads = 1;
        boards.resize(threads);
    }

    bool load() {
        Board board;
        for (const std::string& path : config.files) {
            std::ifstream in(path);
            if (!in) {
                std::cout << "cannot open " << path << std::endl;
                return false;
            }
            std::vector<Entry> fileEntries;
            std::string line;
            size_t bad = 0;
            while (std::getline(in, line)) {
                Entry e;
                if (parseLine(line, board, e)) fileEntries.push_back(e);
                else if (!line.empty()) bad++;
            }
            // datagen writes the positions of a game next to each other, so taking
            // the end of the file keeps whole games out of training. A random split
            // would put positions of the same game (same result) on both sides and
            // make the validation error look better than it is.
            size_t split = (size_t)(fileEntries.size() * (1.0 - config.validFraction));
            train.insert(train.end(), fileEntries.begin(), fileEntries.begin() + split);
            valid.insert(valid.end(), fileEntries.begin() + split, fileEntries.end());
            std::cout << path << ": " << fileEntries.size() << " positions";
            if (bad) std::cout << " (" << bad << " unreadable lines skipped)";
            std::cout << std::endl;
        }
        if (config.maxPositions && train.size() > config.maxPositions) {
            std::mt19937_64 rng(config.seed);
            std::shuffle(train.begin(), train.end(), rng);
            train.resize(config.maxPositions);
            size_t maxValid = std::max<size_t>(1, (size_t)(config.maxPositions * config.validFraction));
            if (valid.size() > maxValid) {
                std::shuffle(valid.begin(), valid.end(), rng);
                valid.resize(maxValid);
            }
            std::cout << "using a random subset of " << train.size() << " positions" << std::endl;
        }
        if (train.empty()) {
            std::cout << "no positions loaded" << std::endl;
            return false;
        }
        return true;
    }

    // Finds the K that best maps the current evaluation to game results.
    double fitK() {
        std::vector<int> evals(train.size());
        parallel(train.size(), [&](int t, size_t begin, size_t end) {
            for (size_t i = begin; i < end; i++) evals[i] = evalWhite(boards[t], train[i]);
        });
        auto err = [&](double K) {
            double sum = 0;
            for (size_t i = 0; i < train.size(); i++) {
                double d = train[i].result - sigmoid(K, evals[i]);
                sum += d * d;
            }
            return sum / train.size();
        };
        // Golden-section search; the error is unimodal in K.
        double a = 0.05, b = 5.0;
        const double g = (std::sqrt(5.0) - 1) / 2;
        double c = b - g * (b - a), d = a + g * (b - a);
        double fc = err(c), fd = err(d);
        for (int i = 0; i < 40; i++) {
            if (fc < fd) { b = d; d = c; fd = fc; c = b - g * (b - a); fc = err(c); }
            else         { a = c; c = d; fc = fd; d = a + g * (b - a); fd = err(d); }
        }
        return (a + b) / 2;
    }

    void setTargets(double K) {
        for (std::vector<Entry>* set : {&train, &valid}) {
            for (Entry& e : *set) {
                e.target = (float)(config.lambda * e.result +
                                   (1 - config.lambda) * sigmoid(K, e.search));
            }
        }
    }

    // Mean squared error of the current weights over one set of positions.
    double error(double K, const std::vector<Entry>& set) {
        if (set.empty()) return 0;
        std::vector<double> sums(threads, 0.0);
        parallel(set.size(), [&](int t, size_t begin, size_t end) {
            double sum = 0;
            for (size_t i = begin; i < end; i++) {
                double d = set[i].target - sigmoid(K, evalWhite(boards[t], set[i]));
                sum += d * d;
            }
            sums[t] = sum;
        });
        double total = 0;
        for (double s : sums) total += s;   // fixed order: same weights give the same error
        return total / set.size();
    }

    int threadCount() const { return threads; }

private:
    const TuneConfig& config;
    int threads;
    std::vector<Board> boards;   // one scratch board per thread

    static int evalWhite(Board& board, const Entry& e) {
        board.loadForEval(e.pieces, (Color)e.stm, e.castling, e.ep);
        int score = EngineEval::evaluate(board);
        return e.stm == WHITE ? score : -score;
    }

    // Splits [0, n) into one contiguous chunk per thread.
    template <typename Fn>
    void parallel(size_t n, Fn fn) {
        std::vector<std::thread> pool;
        for (int t = 0; t < threads; t++) {
            size_t begin = n * t / threads, end = n * (t + 1) / threads;
            pool.emplace_back([&fn, t, begin, end] { fn(t, begin, end); });
        }
        for (std::thread& th : pool) th.join();
    }
};

void save(const std::string& prefix) {
    saveParams(prefix + ".txt", evalParams);
    saveParamsCpp(prefix + "_EvalParams.txt", evalParams);
}

}  // namespace

TuneConfig parseTuneArgs(const std::vector<std::string>& args) {
    TuneConfig config;
    for (const std::string& token : args) {
        size_t eq = token.find('=');
        if (eq == std::string::npos) {
            config.files.push_back(token);
            continue;
        }
        std::string key = token.substr(0, eq), value = token.substr(eq + 1);
        if (key == "threads") config.threads = std::stoi(value);
        else if (key == "lambda") config.lambda = std::stod(value);
        else if (key == "max") config.maxPositions = std::stoull(value);
        else if (key == "iters") config.iterations = std::stoi(value);
        else if (key == "out") config.out = value;
        else if (key == "seed") config.seed = std::stoull(value);
        else if (key == "valid") config.validFraction = std::max(0.0, std::min(0.5, std::stod(value)));
        else if (key == "patience") config.patience = std::stoi(value);
        else std::cout << "unknown option " << key << std::endl;
    }
    return config;
}

// Output files (prefix = TuneConfig::out):
//   <prefix>.txt / <prefix>_EvalParams.txt            current weights, saved often
//   <prefix>_iterN.txt                                 weights after iteration N
//   <prefix>_best.txt / <prefix>_best_EvalParams.txt   lowest validation error so far
void runTuner(const TuneConfig& config) {
    if (!paramFieldsComplete()) return;
    if (config.files.empty()) {
        std::cout << "usage: tune [threads=N] [lambda=X] [max=N] [iters=N] [out=NAME] [seed=N]"
                     " [valid=X] [patience=N] files..." << std::endl;
        return;
    }
    GenerateMoves init;   // builds the attack tables the evaluation uses
    EngineEval::usePawnHash = false;

    Tuner tuner(config);
    auto start = Clock::now();
    if (!tuner.load()) return;
    bool validate = !tuner.valid.empty();
    std::cout << tuner.train.size() << " training and " << tuner.valid.size()
              << " validation positions loaded in " << std::fixed << std::setprecision(1)
              << seconds(start) << " s, " << tuner.threadCount() << " threads" << std::endl;

    double K = tuner.fitK();
    tuner.setTargets(K);
    auto t0 = Clock::now();
    double best = tuner.error(K, tuner.train);
    double perEval = seconds(t0);
    double bestValid = tuner.error(K, tuner.valid);
    std::cout << std::setprecision(4) << "K = " << K << std::setprecision(7)
              << ", start error " << best;
    if (validate) std::cout << " (validation " << bestValid << ")";
    std::cout << std::setprecision(2) << ", one pass over the data takes " << perEval << " s"
              << std::endl;

    int* p = paramData(evalParams);
    // Weights that never change the error (for example pawn squares on the first
    // and last rank) are skipped; they are rechecked every few iterations.
    std::vector<bool> dead(PARAM_COUNT, false);
    // Only the pawn value is fixed, as the unit the other weights are measured in.
    // A piece's material value and its PST overlap (adding c to one and -c to every
    // square of the other gives the same evaluation), but tuning both is still
    // useful: one material step moves a piece's value as much as 64 PST steps.
    // The king danger entries from KING_SAFETY_FROZEN_FROM on are reached so rarely
    // that tuning only fits noise: it walked them from 500 to over 2000 one step at
    // a time, which also made each iteration take hours. They stay as they are.
    const int KING_SAFETY_FROZEN_FROM = 55;
    std::vector<bool> frozen(PARAM_COUNT, false);
    int tunable = 0;
    for (int i = 0; i < PARAM_COUNT; i++) {
        std::string name = paramName(i);
        frozen[i] = name.rfind("material[0]", 0) == 0;
        if (name.rfind("kingSafetyTable[", 0) == 0 && std::stoi(name.substr(16)) >= KING_SAFETY_FROZEN_FROM)
            frozen[i] = true;
        if (!frozen[i]) tunable++;
    }
    std::cout << tunable << " of " << PARAM_COUNT << " weights are tuned (pawn material and rare "
              << "kingSafetyTable entries are fixed)" << std::endl;
    save(config.out + "_best");   // the starting weights are the best known so far
    int itersWithoutBest = 0;

    for (int iter = 1; iter <= config.iterations; iter++) {
        auto iterStart = Clock::now();
        bool recheck = (iter % 5 == 1);
        int changed = 0, active = 0;
        double iterStartError = best;

        for (int i = 0; i < PARAM_COUNT; i++) {
            if (frozen[i] || (dead[i] && !recheck)) continue;
            active++;
            int original = p[i];
            bool improved = false;
            bool flat = true;
            for (int dir : {+1, -1}) {
                p[i] = original + dir;
                double e = tuner.error(K, tuner.train);
                if (e != best) flat = false;
                if (e < best) {
                    best = e;
                    improved = true;
                    // Keep going in the same direction while it helps.
                    for (;;) {
                        p[i] += dir;
                        double e2 = tuner.error(K, tuner.train);
                        if (e2 < best) best = e2;
                        else { p[i] -= dir; break; }
                    }
                    break;
                }
            }
            if (!improved) p[i] = original;
            else changed++;
            dead[i] = flat;

            if (active % 50 == 0) {
                std::cout << "  iter " << iter << "  " << i + 1 << "/" << PARAM_COUNT
                          << "  error " << std::setprecision(7) << best
                          << "  changed " << changed << std::setprecision(0)
                          << "  " << seconds(start) << " s" << std::endl;
                save(config.out);
            }
        }
        save(config.out);
        saveParams(config.out + "_iter" + std::to_string(iter) + ".txt", evalParams);

        std::cout << "iter " << iter << " done: error " << std::setprecision(7) << iterStartError
                  << " -> " << best;
        if (validate) {
            double v = tuner.error(K, tuner.valid);
            std::cout << ", validation " << v;
            if (v < bestValid) {
                bestValid = v;
                itersWithoutBest = 0;
                save(config.out + "_best");
                std::cout << " (new best)";
            } else {
                itersWithoutBest++;
                std::cout << " (best " << bestValid << ")";
            }
        }
        std::cout << ", " << changed << " weights changed, " << std::setprecision(0)
                  << seconds(iterStart) << " s" << std::endl;

        if (changed == 0) {
            std::cout << "no weight changed, finished" << std::endl;
            break;
        }
        if (validate && itersWithoutBest >= config.patience) {
            std::cout << "validation error has not improved for " << itersWithoutBest
                      << " iterations, stopping; best weights are in " << config.out
                      << "_best.txt" << std::endl;
            break;
        }
    }
    EngineEval::usePawnHash = true;
    PTT.clear();
}
