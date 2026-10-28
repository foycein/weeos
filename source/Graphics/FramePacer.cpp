#include "Graphics/FramePacer.h"

#include <algorithm>
#include <cmath>

namespace Graphics
{

FramePacer::FramePacer(double timeAtStart)
    : startSeconds(timeAtStart)
{
}

int FramePacer::TakeDueSteps(double nowSeconds)
{
    std::int64_t stepsElapsed = StepsElapsedAt(nowSeconds);
    std::int64_t stepsOwed = stepsElapsed - stepsAccountedFor;
    if (stepsOwed <= 0)
        return 0;

    std::int64_t stepsToRun = std::min<std::int64_t>(stepsOwed, MaximumStepsPerRefresh);
    stepsDropped += stepsOwed - stepsToRun;
    stepsAccountedFor = stepsElapsed;
    return static_cast<int>(stepsToRun);
}

double FramePacer::PartialStep(double nowSeconds) const
{
    double stepsElapsedExactly = (nowSeconds - startSeconds) / StepSeconds;
    double partialStep = stepsElapsedExactly - static_cast<double>(stepsAccountedFor);
    return std::clamp(partialStep, 0.0, 1.0);
}

std::int64_t FramePacer::StepsTaken() const
{
    return stepsAccountedFor - stepsDropped;
}

std::int64_t FramePacer::StepsDropped() const
{
    return stepsDropped;
}

std::int64_t FramePacer::StepsElapsedAt(double nowSeconds) const
{
    double secondsSinceStart = std::max(nowSeconds - startSeconds, 0.0);
    return static_cast<std::int64_t>(std::floor(secondsSinceStart / StepSeconds));
}

}
