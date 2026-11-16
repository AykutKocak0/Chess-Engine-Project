#include "Datagen.h"
#include "Board.h"
#include "Search.h"
#include "GenerateMoves.h"
#include "TransPositionTable.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <random>
#include <sstream>
#include <vector>

namespace {

// Random plies played before the engine takes over (8 or 9, so both colors
// start the "real" game equally often).
constexpr int OPENING_PLIES_MIN = 8;
// Openings this unbalanced after the random moves are thrown away.
constexpr int OPENING_MAX_SCORE = 1000;
// Win adjudication: |score| >= this for WIN_PLIES plies in a row.
constexpr int WIN_SCORE = 2000;
constexpr int WIN_PLIES = 4;
// Draw adjudication: after DRAW_START_PLY, |score| <= DRAW_SCORE for DRAW_PLIES plies in a row.
constexpr int DRAW_START_PLY = 80;
constexpr int DRAW_SCORE = 10;
constexpr int DRAW_PLIES = 10;
// Games longer than this are scored as draws.
constexpr int MAX_GAME_PLIES = 400;

enum class Outcome { None, WhiteWin, BlackWin, Draw };

struct Sample {
    std::string epd;  // first four FEN fields
    int score;        // search score, side to move's point of view
};

int legalMoves(Board& board, Move* out) {
    Move pseudo[256];
    int n = GenerateMoves::generateMoves(board, pseudo);
    int count = 0;
    for (int i = 0; i < n; i++) {
        if (board.makeMove(pseudo[i])) {   // makeMove undoes illegal moves itself
            board.unmakeMove(pseudo[i]);
            out[count++] = pseudo[i];
        }
    }
    return count;
}

// Same position three times since the last capture or pawn move.
bool isThreefold(Board& board) {
    uint64_t key = board.getZobristKey();
    int limit = std::max(0, board.historyCount - 1 - board.getMovesWH());
    int seen = 1;
    for (int i = board.historyCount - 3; i >= limit; i -= 2) {
        if (board.historyStack[i] == key && ++seen >= 3) return true;
    }
    return false;
}

// K vs K, or a single minor piece on the board.
bool insufficientMaterial(const Board& board) {
    uint64_t heavyOrPawns = board.getPieceBB(W_PAWN) | board.getPieceBB(B_PAWN) |
                            board.getPieceBB(W_ROOK) | board.getPieceBB(B_ROOK) |
                            board.getPieceBB(W_QUEEN) | board.getPieceBB(B_QUEEN);
    if (heavyOrPawns) return false;
    uint64_t minors = board.getPieceBB(W_KNIGHT) | board.getPieceBB(B_KNIGHT) |
                      board.getPieceBB(W_BISHOP) | board.getPieceBB(B_BISHOP);
    return __builtin_popcountll(minors) <= 1;
}

std::string epdOf(const Board& board) {
    // Keep board, side, castling and en passant; drop the two move counters.
    std::string fen = board.toFEN();
    std::istringstream in(fen);
    std::string a, b, c, d;
    in >> a >> b >> c >> d;
    return a + " " + b + " " + c + " " + d;
}

const char* resultString(Outcome o) {
    switch (o) {
        case Outcome::WhiteWin: return "1-0";
        case Outcome::BlackWin: return "0-1";
        default:                return "1/2-1/2";
    }
}

// Plays one game. Returns Outcome::None if the random opening was unusable.
Outcome playGame(Search& search, Board& board, std::mt19937_64& rng,
                 const DatagenConfig& config, std::vector<Sample>& samples) {
    samples.clear();
    TT.clear();
    board.setupStartingBoard();

    SearchLimits limits;
    limits.nodes = (uint64_t)config.nodes;

    Move moves[256];
    int openingPlies = OPENING_PLIES_MIN + (int)(rng() % 2);
    for (int i = 0; i < openingPlies; i++) {
        int n = legalMoves(board, moves);
        if (n == 0) return Outcome::None;
        board.makeMove(moves[rng() % n]);
    }
    if (legalMoves(board, moves) == 0) return Outcome::None;
    SearchResult check = search.searchSync(board, limits);
    if (!check.hasMove || std::abs(check.score) > OPENING_MAX_SCORE) return Outcome::None;

    int winStreak = 0;   // signed: >0 White winning streak, <0 Black
    int drawStreak = 0;
    for (int ply = openingPlies; ; ply++) {
        int n = legalMoves(board, moves);
        if (n == 0) {
            if (!board.isInCheck()) return Outcome::Draw;   // stalemate
            return board.sideToMove == WHITE ? Outcome::BlackWin : Outcome::WhiteWin;
        }
        if (board.getMovesWH() >= 100 || isThreefold(board) || insufficientMaterial(board) ||
            ply >= MAX_GAME_PLIES) {
            return Outcome::Draw;
        }

        SearchResult r = search.searchSync(board, limits);
        int whiteScore = board.sideToMove == WHITE ? r.score : -r.score;

        // Adjudication.
        if (whiteScore >= WIN_SCORE) winStreak = winStreak > 0 ? winStreak + 1 : 1;
        else if (whiteScore <= -WIN_SCORE) winStreak = winStreak < 0 ? winStreak - 1 : -1;
        else winStreak = 0;
        if (winStreak >= WIN_PLIES) return Outcome::WhiteWin;
        if (winStreak <= -WIN_PLIES) return Outcome::BlackWin;

        if (ply >= DRAW_START_PLY && std::abs(whiteScore) <= DRAW_SCORE) drawStreak++;
        else drawStreak = 0;
        if (drawStreak >= DRAW_PLIES) return Outcome::Draw;

        // Quiet positions only: the static evaluation is what gets tuned, so the
        // position should not hinge on a capture or a check that is still pending.
        bool quiet = !board.isInCheck() && !r.move.isCapture() && !r.move.isPromotion() && !r.isMate;
        if (quiet) samples.push_back({epdOf(board), r.score});

        board.makeMove(r.move);
    }
}

}  // namespace

void runDatagen(const DatagenConfig& config, Search& search) {
    std::ofstream out(config.outFile, std::ios::app);
    if (!out) {
        uciPrint("info string datagen: cannot open " + config.outFile);
        return;
    }

    std::mt19937_64 rng(config.seed);
    Board board;
    std::vector<Sample> samples;
    auto start = std::chrono::steady_clock::now();
    int gamesDone = 0, discarded = 0;
    long long written = 0;
    int results[3] = {0, 0, 0};   // white wins, black wins, draws

    while (gamesDone < config.games) {
        Outcome outcome = playGame(search, board, rng, config, samples);
        if (outcome == Outcome::None) {
            discarded++;
            continue;
        }
        gamesDone++;
        results[outcome == Outcome::WhiteWin ? 0 : outcome == Outcome::BlackWin ? 1 : 2]++;

        // Neighbouring positions of one game are nearly identical; keep a random subset.
        std::shuffle(samples.begin(), samples.end(), rng);
        int keep = std::min((int)samples.size(), config.positionsPerGame);
        for (int i = 0; i < keep; i++) {
            out << samples[i].epd << " c9 \"" << resultString(outcome) << "\"; ce "
                << samples[i].score << ";\n";
        }
        written += keep;

        if (gamesDone % 100 == 0 || gamesDone == config.games) {
            out.flush();
            double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            std::ostringstream msg;
            msg << "info string datagen games " << gamesDone << "/" << config.games
                << " positions " << written
                << " W/B/D " << results[0] << "/" << results[1] << "/" << results[2]
                << " discarded " << discarded
                << " games/s " << (secs > 0 ? gamesDone / secs : 0.0);
            uciPrint(msg.str());
        }
    }
    uciPrint("info string datagen done, file " + config.outFile);
}
