#include <iostream>
#include <string>
#include <sstream>
#include <vector>
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
            uciPrint("uciok");
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