#include "Platform/PreciseTimer.h"

#include <windows.h>

namespace Platform
{

namespace
{

std::int64_t ReadCounter()
{
    LARGE_INTEGER count;
    QueryPerformanceCounter(&count);
    return count.QuadPart;
}

}

PreciseTimer::PreciseTimer()
    : startCount(ReadCounter())
{
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    countsPerSecond = static_cast<double>(frequency.QuadPart);
}

double PreciseTimer::ElapsedSeconds() const
{
    return static_cast<double>(ReadCounter() - startCount) / countsPerSecond;
}

}
