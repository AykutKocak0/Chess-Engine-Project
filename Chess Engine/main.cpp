#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include "Board.h"
#include "Search.h"
#include "GenerateMoves.h"
#include "Constants.h"
#include "TransPositionTable.h"
#include <chrono>
#include "EngineEval.h"
#include "Datagen.h"
#include "EvalParams.h"
#include "Tuner.h"

// datagen <games> <nodes> <outfile> [seed] [positionsPerGame]
DatagenConfig parseDatagen(std::istream& in) {
    DatagenConfig config;
    in >> config.games >> config.nodes >> config.outFile;
    uint64_t seed;
    if (in >> seed) config.seed = seed;
    int perGame;
    if (in >> perGame) config.positionsPerGame = perGame;
    return config;
}

// Fixed positions for "bench": searched to a fixed depth with fresh tables, so the
// total node count is the same on every run and every machine. It only changes when
// the search or evaluation changes, which makes it a quick check after an edit.
static const char* BENCH_FENS[] = {
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
    "r1bq1rk1/pp2bppp/2n1pn2/2pp4/3P4/2PBPN2/PP1N1PPP/R1BQ1RK1 w - - 0 8",
    "r3k2r/2pb1ppp/2pp1q2/p7/1nP1B3/1P2P3/P2N1PPP/R2QK2R w KQkq a6 0 14",
    "4rrk1/2p1b1p1/p1p3q1/4p3/2P2n1p/1P1NR2P/PB3PP1/3R1QK1 b - - 2 24",
    "r3qbrk/6p1/2b2pPp/p3pP1Q/PpPpP2P/3P1B2/2PB3K/R5R1 w - - 16 42",
    "6k1/1R3p2/6p1/2Bp3p/3P2q1/P7/1P2rQ1K/5R2 b - - 4 44",
    "8/8/1p2k1p1/3p3p/1p1P1P1P/1P2PK2/8/8 w - - 3 54",
    "8/3p4/p1bk3p/Pp6/1Kp1PpPp/2P2P1P/2P5/5B2 b - - 1 33",
    "2K5/p7/7P/5pR1/8/5k2/r7/8 w - - 0 1",
};

void runBench(Search& search, int depth) {
    uint64_t totalNodes = 0;
    auto start = std::chrono::high_resolution_clock::now();
    for (const char* fen : BENCH_FENS) {
        TT.clear();
        PTT.clear();
        Board board;
        board.parseFEN(fen);
        SearchLimits limits;
        limits.depth = depth;
        SearchResult result = search.searchSync(board, limits);
        totalNodes += search.getNodes();
        std::cout << search.moveToString(result.move) << "  " << search.getNodes() << "  " << fen << std::endl;
    }
    std::chrono::duration<double> elapsed = std::chrono::high_resolution_clock::now() - start;
    TT.clear();
    PTT.clear();
    std::cout << "Bench: " << totalNodes << " nodes  " << (int)(elapsed.count() * 1000) << " ms  "
              << (uint64_t)(totalNodes / std::max(elapsed.count(), 0.001)) << " nps" << std::endl;
}

// Standard perft positions with known node counts. Any FAIL means move generation
// (or make/unmake) is broken.
bool runPerftCheck() {
    struct PerftCase { const char* fen; int depth; uint64_t nodes; };
    static const PerftCase cases[] = {
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 5, 4865609ULL},
        {"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 4, 4085603ULL},
        {"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 5, 674624ULL},
        {"r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 4, 422333ULL},
        {"rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 4, 2103487ULL},
        {"r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", 4, 3894594ULL},
    };
    GenerateMoves gen;
    bool allOk = true;
    for (const PerftCase& c : cases) {
        Board board;
        board.parseFEN(c.fen);
        uint64_t nodes = board.Perft(c.depth, gen);
        bool ok = nodes == c.nodes;
        allOk = allOk && ok;
        std::cout << (ok ? "PASS " : "FAIL ") << "d" << c.depth << " " << nodes
                  << (ok ? "" : " expected " + std::to_string(c.nodes)) << "  " << c.fen << std::endl;
    }
    std::cout << (allOk ? "perft: all passed" : "perft: FAILED") << std::endl;
    return allOk;
}

Move parseMove(Board& board, std::string moveStr, Search& search) {
    GenerateMoves gen;
    Move moves[256];
    int count = gen.generateMoves(board, moves);

    for (int i = 0; i < count; i++) {
        if (search.moveToString(moves[i]) == moveStr) {
            return moves[i];
        }
    }
    return Move();
}

void handlePosition(Board& board, std::stringstream& ss, Search& search) {
    std::string type;
    ss >> type;

    if (type == "startpos") {
        board.setupStartingBoard();
        std::string next;
        ss >> next;
    } 
    else if (type == "fen") {
        std::string fenString;
        std::string part;
        
        for (int i = 0; i < 6; i++) {
            if (ss >> part) {
                if (i > 0) fenString += " ";
                fenString += part;
            }
        }
        
        board.parseFEN(fenString);
    }

    std::string currentWord;
    while (ss >> currentWord) {
        if (currentWord == "moves") continue; 
        
        Move m = parseMove(board, currentWord, search);
        if (m.from != m.to) { 
             
            board.makeMove(m);
            
        }
    }
    
}

int main(int argc, char* argv[]) {
    Board board;
    Search search;
    std::string line, command;
    GenerateMoves gen;
    EngineEval eval;

    int argi = 1;
    // Start with tuned weights instead of the compiled-in ones:
    //   Botkut.exe params tuned.txt            (then UCI as usual)
    //   Botkut.exe params tuned.txt tune ...   (continue tuning from them)
    if (argc > argi + 1 && std::string(argv[argi]) == "params") {
        if (!loadParams(argv[argi + 1], evalParams)) return 1;
        argi += 2;
    }

    if (argc > argi) {
        std::string mode = argv[argi];
        std::stringstream args;
        for (int i = argi + 1; i < argc; i++) args << argv[i] << " ";
        // Several datagen processes can run in parallel:
        //   Botkut.exe datagen 10000 5000 data_1.epd 1
        if (mode == "datagen") {
            runDatagen(parseDatagen(args), search);
            return 0;
        }
        //   Botkut.exe tune threads=6 data_1.epd data_2.epd
        if (mode == "tune") {
            runTuner(parseTuneArgs(std::vector<std::string>(argv + argi + 1, argv + argc)));
            return 0;
        }
        //   Botkut.exe bench [depth]     Botkut.exe perftcheck
        if (mode == "bench") {
            int depth = 10;
            args >> depth;
            runBench(search, depth);
            return 0;
        }
        if (mode == "perftcheck") {
            return runPerftCheck() ? 0 : 1;
        }
    }

    // The search runs on its own thread (see Search::startAsync), so this loop keeps
    // reading commands while it thinks and "stop" / "isready" are answered at once.
    while (std::getline(std::cin, line)) {
        std::stringstream ss(line);
        command.clear();   // an empty line must not repeat the previous command
        ss >> command;

        if (command == "uci") {
            uciPrint("id name Botkut");
            uciPrint("id author Aykut");
            uciPrint("option name Hash type spin default 64 min 1 max 4096");
            uciPrint("option name Clear Hash type button");
            uciPrint("uciok");
        }
        else if (command == "setoption") {
            // setoption name <id> [value <x>]; the name may contain spaces ("Clear Hash").
            std::string word, name, value;
            bool inValue = false;
            while (ss >> word) {
                if (word == "name") { inValue = false; continue; }
                if (word == "value") { inValue = true; continue; }
                std::string& target = inValue ? value : name;
                if (!target.empty()) target += " ";
                target += word;
            }
            std::string key = name;   // option names are case-insensitive
            std::transform(key.begin(), key.end(), key.begin(), ::tolower);
            search.stopAndWait();   // the search thread uses the table
            if (key == "hash") {
                int mb = 0;
                try { mb = std::stoi(value); } catch (...) { mb = 0; }
                if (mb >= 1 && mb <= 4096) TT.resize(mb);
                else uciPrint("info string Hash must be between 1 and 4096 MB");
            }
            else if (key == "clear hash") {
                TT.clear();
                PTT.clear();
            }
            else {
                uciPrint("info string unknown option " + name);
            }
        }
        else if (command == "ucinewgame") {
            search.stopAndWait();   // the search thread uses both tables
            TT.clear();
            PTT.clear();
        }
        else if (command == "isready") {
            uciPrint("readyok");
        }
        else if (command == "position") {
            // Safe during a search: the search thread works on its own copy of the board.
            handlePosition(board, ss, search);
        }
        else if (command == "go") {
            SearchLimits limits;
            std::string subCommand;
            int whiteTime = 0;
            int blackTime = 0;
            int whiteIncrement = 0;
            int blackIncrement = 0;

            while (ss >> subCommand) {
                if (subCommand == "depth") ss >> limits.depth;
                else if (subCommand == "wtime") ss >> whiteTime;
                else if (subCommand == "btime") ss >> blackTime;
                else if (subCommand == "winc") ss >> whiteIncrement;
                else if (subCommand == "binc") ss >> blackIncrement;
                else if (subCommand == "movestogo") ss >> limits.movesToGo;
                else if (subCommand == "movetime") ss >> limits.moveTime;
                else if (subCommand == "nodes") ss >> limits.nodes;
                else if (subCommand == "infinite") limits.infinite = true;
            }
            if (limits.depth < 1) limits.depth = 1;
            if (limits.depth > 64) limits.depth = 64;
            limits.timeLeft = (board.sideToMove == WHITE) ? whiteTime : blackTime;
            limits.increment = (board.sideToMove == WHITE) ? whiteIncrement : blackIncrement;
            search.startAsync(board, limits);
        }
        else if (command == "stop") {
            search.stopAndWait();
        }
        else if (command == "quit") {
            search.stopAndWait();
            break;
        }
        else if (command == "fen") {
            uciPrint(board.toFEN());
        }
        else if (command == "datagen") {
            search.stopAndWait();
            runDatagen(parseDatagen(ss), search);
        }
        else if(command == "eval"){
            search.stopAndWait();   // evaluation uses the shared pawn hash table
            Color originalSide = board.sideToMove;
            board.sideToMove = WHITE;
            int sWhite = eval.evaluate(board);
            std::cout<<"WHITE PERSPECTIVE: "<<sWhite<<std::endl;
            board.sideToMove = BLACK;
            int sBlack = eval.evaluate(board);
            std::cout<<"BLACK PERSPECTIVE: "<<sBlack<<std::endl;
            board.sideToMove = originalSide;
        }
        else if (command == "perft"){
            search.stopAndWait();   // don't share the CPU with a running search
            int depth = 1;
            if (ss >> depth) { /* gets depth from 'perft 5' */ }
            auto start = std::chrono::high_resolution_clock::now();
            uint64_t nodes = board.Perft(depth,gen); 
            auto end = std::chrono::high_resolution_clock::now();

            std::chrono::duration<double> elapsed = end - start;
            double nps = nodes / elapsed.count();

            std::cout << "Nodes: " << nodes << " NPS: " << (uint64_t)nps << std::endl;
        }
        else if (command == "bench") {
            search.stopAndWait();
            int depth = 10;
            ss >> depth;
            runBench(search, depth);
        }
        else if (command == "perftcheck") {
            search.stopAndWait();
            runPerftCheck();
        }
        else if (command == "searchdebug"){
            //search.startSearch(board, 8);
            std::cout<<"BOARD PAWN KEY: "<<board.getPawnKey()<<std::endl;
        }
        else if (command == "checklegal") {
            std::string moveStr;
            ss >> moveStr; 
            
            Move move = parseMove(board,moveStr,search); 
            
            uciPrint(board.isLegalMove(move) ? "check_result legal" : "check_result illegal");
        }
    }
    // End of input: let a bounded search (depth/time) finish so piped commands such as
    // "go depth 10" still produce their result; an infinite search could never end.
    if (search.isInfinite()) search.stopAndWait();
    else search.waitForFinish();
    return 0;
}