#include "TimeManagement.h"


void TimeManagement::startTimer(int timeLeft, int increment, int movesToGo, int phase) {
    startTime = std::chrono::high_resolution_clock::now();
    if(movesToGo <= 0){
        movesToGo = 35;
    }

    int baseTimeMs = static_cast<int>((timeLeft / movesToGo) + (increment * 0.6));
    
    double timeMultiplier = 1.3 - (0.5 * phase / 256.0);

    allocatedTimeMs = static_cast<int>(baseTimeMs * timeMultiplier);
    if (allocatedTimeMs >= timeLeft) {
        allocatedTimeMs = timeLeft - 50; 
    }
    if (allocatedTimeMs < 0) allocatedTimeMs = 10; 
}

bool TimeManagement::isTimeUp() {
    return getElapsedTimeMs() >= allocatedTimeMs;
}

int TimeManagement::getElapsedTimeMs() {
    auto now = std::chrono::high_resolution_clock::now();
    return static_cast<int> (std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count());
}