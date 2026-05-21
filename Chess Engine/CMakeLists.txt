cmake_minimum_required(VERSION 3.10)
project(Botkut)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(Botkut
    main.cpp 
    Board.cpp 
    Search.cpp 
    GenerateMoves.cpp 
    Constants.cpp
    EngineEval.cpp
    TransPositionTable.cpp
    TimeManagement.cpp
)