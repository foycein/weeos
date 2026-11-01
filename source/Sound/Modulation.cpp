#include "Sound/Modulation.h"

#include <cmath>
#include <numbers>

namespace Sound
{

namespace
{

constexpr float MillisecondsPerSecond = 1000.0f;

}

ModulationTarget ModulationTargetFromNumber(std::uint8_t number)
{
    constexpr std::uint8_t HighestKnownTarget = static_cast<std::uint8_t>(ModulationTarget::Pan);

    if (number > HighestKnownTarget)
        return ModulationTarget::None;

    return static_cast<ModulationTarget>(number);
}

int ModulationSineNumber(int step)
{
    double angle = 2.0 * std::numbers::pi * double(step) / double(StepsInOneCycle);
    return static_cast<int>(std::lround(double(HighestSineNumber) * std::sin(angle)));
}

float Modulation::Value(const ModulationSettings& settings) const
{
    if (settings.depth == 0.0f || elapsedMilliseconds < settings.delayMilliseconds)
        return 0.0f;

    int step = static_cast<int>(float(StepsInOneCycle) * cyclePosition);
    return float(ModulationSineNumber(step)) / HighestSineNumber * settings.depth * float(settings.range);
}

void Modulation::Advance(const ModulationSettings& settings, int milliseconds)
{
    if (elapsedMilliseconds < settings.delayMilliseconds)
    {
        if (elapsedMilliseconds + milliseconds <= settings.delayMilliseconds)
        {
            elapsedMilliseconds += milliseconds;
            return;
        }

        milliseconds -= settings.delayMilliseconds - elapsedMilliseconds;
        elapsedMilliseconds = settings.delayMilliseconds;
    }

    cyclePosition += settings.speed * float(milliseconds) / MillisecondsPerSecond;
    float wholeCycles = std::trunc(cyclePosition);
    cyclePosition -= wholeCycles;
}

}
