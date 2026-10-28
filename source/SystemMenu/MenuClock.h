#pragma once

#include <cstdint>

namespace SystemMenu
{

// The time of the menu, counted in steps. A step is the time of one picture of the console, 1001/60000 of a second.
// The screens measure their waits with this clock, so a test can run a minute of the menu in no time.
class MenuClock
{
public:
    static constexpr double SecondsPerStep = 1001.0 / 60000.0;

    void Advance()
    {
        ++stepCount;
    }

    double Seconds() const
    {
        return static_cast<double>(stepCount) * SecondsPerStep;
    }

private:
    std::int64_t stepCount = 0;
};

}
