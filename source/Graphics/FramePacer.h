#pragma once

#include <cstdint>

namespace Graphics
{

// Keeps the program in step with the console. The design has three clocks:
//
//  - The simulation clock ticks in steps of 1001/60000 seconds, the 59.94 Hz of the console. One step is everything the
//    console does for one picture: input, menu logic, animations. This class is that clock. It counts the steps from
//    the elapsed time, never from the number of pictures shown.
//  - The display clock is the refresh of the monitor. Present waits for it. At each refresh the program asks this
//    class which steps are due (none, one or at most two), runs them, then draws the newest state.
//  - The audio clock belongs to the Sound part and runs on its own thread. The three clocks never wait for each other.
class FramePacer
{
public:
    static constexpr double StepSeconds = 1001.0 / 60000.0;

    // The most steps handed out at once. When more are due, for example after the program was stopped for a while,
    // the others are dropped instead of being run in a hurry.
    static constexpr int MaximumStepsPerRefresh = 2;

    explicit FramePacer(double timeAtStart);

    // Gives the number of steps that became due since the last call. The time comes from a PreciseTimer.
    int TakeDueSteps(double nowSeconds);

    // How far the time has come into the next step: 0 when a step has just become due, up to just below 1.
    // A program on a fast display can draw animations at this fraction of a frame.
    double PartialStep(double nowSeconds) const;

    // The steps handed out and the steps dropped, all together since the start.
    std::int64_t StepsTaken() const;
    std::int64_t StepsDropped() const;

private:
    // The whole steps that fit in the time since the start.
    std::int64_t StepsElapsedAt(double nowSeconds) const;

    double startSeconds;
    std::int64_t stepsAccountedFor = 0;
    std::int64_t stepsDropped = 0;
};

}
