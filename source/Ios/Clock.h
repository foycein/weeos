#pragma once

#include <chrono>

namespace Ios
{

// The time of day. The services that need it ask a clock, so a test can set the time.
class Clock
{
public:
    virtual ~Clock() = default;

    // The time in UTC.
    virtual std::chrono::system_clock::time_point Now() const = 0;
};

// The clock of the PC.
class SystemClock final : public Clock
{
public:
    std::chrono::system_clock::time_point Now() const override
    {
        return std::chrono::system_clock::now();
    }
};

}
