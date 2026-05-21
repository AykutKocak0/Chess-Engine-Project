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

    while (std::getline(std::cin, line)) {
        std::stringstream ss(line);
        ss >> command;

        if (command == "uci") {
            std::cout << "id name Botkut" << std::endl;
            std::cout << "id author Aykut" << std::endl;
            std::cout << "uciok" << std::endl;
        }
        else if (command == "ucinewgame") {
            TT.clear();
            PTT.clear();
        }
        else if (command == "isready") {
            TT.clear();
            std::cout << "readyok" << std::endl;
        } 
        else if (command == "position") {
            handlePosition(board, ss, search);
        } 
        else if (command == "go") {
            int depth = 64;
            std::string subCommand;
            int whiteTime = 0;
            int blackTime = 0;
            int movesToGo = 0;
            int whiteIncrement = 0;
            int blackIncrement = 0;
        
            while (ss >> subCommand) {
                if (subCommand == "depth") {
                    ss >> depth;
                }
                if(subCommand == "wtime") {
                    ss >> whiteTime;
                }
                if(subCommand == "btime") {
                    ss >> blackTime;
                }
                if(subCommand == "winc") {
                    ss >> whiteIncrement;
                }
                if(subCommand == "binc") {
                    ss >> blackIncrement;
                }
                if(subCommand == "movestogo") {
                    ss >> movesToGo;
                }

            }
            int timeLeft = (board.sideToMove == WHITE) ? whiteTime : blackTime;
            int increment = (board.sideToMove == WHITE) ? whiteIncrement : blackIncrement;
            search.startSearch(board, depth, timeLeft, increment, movesToGo); 
        }
        else if (command == "stop") {
            search.stop();
        }
        else if (command == "quit") {
            break;
        }
        else if(command == "eval"){
            board.sideToMove = WHITE;
            int sWhite = eval.evaluate(board);
            std::cout<<"WHITE PERSPECTIVE: "<<sWhite<<std::endl;
            board.sideToMove = BLACK;
            int sBlack = eval.evaluate(board);
            std::cout<<"BLACK PERSPECTIVE: "<<sBlack<<std::endl;
        }
        else if (command == "perft"){   
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
            
            if (board.isLegalMove(move)) {
                std::cout << "check_result legal" << std::endl;
            } else {
                std::cout << "check_result illegal" << std::endl;
            }
        }
    }
    return 0;
}