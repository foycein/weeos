#pragma once

#include <cstdint>

namespace Platform
{

// Measures time with the performance counter of the PC. It never jumps backward and its steps are far below a microsecond.
class PreciseTimer
{
public:
    PreciseTimer();

    // The seconds since the timer was made.
    double ElapsedSeconds() const;

private:
    std::int64_t startCount = 0;
    double countsPerSecond = 0;
};

}
