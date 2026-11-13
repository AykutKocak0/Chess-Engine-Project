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

int main() {
    Board board;
    Search search;
    std::string line, command;
    GenerateMoves gen;
    EngineEval eval;

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