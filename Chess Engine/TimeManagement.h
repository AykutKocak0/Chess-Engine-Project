#pragma once
#include <chrono>


class TimeManagement {
private:
    std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
    int allocatedTimeMs;
    int maxTimeMs;

public:
    void startTimer(int timeLeft, int increment, int movesToGo, int phase);
    // Fixed time per move ("go movetime N").
    void startFixed(int moveTimeMs);
    bool isTimeUp();
    int getElapsedTimeMs();
    int getAllocatedTimeMs(){return allocatedTimeMs;}
};