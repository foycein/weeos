#include "Sound/Envelope.h"

#include <array>
#include <cmath>

namespace Sound
{

namespace
{

constexpr double SilentDecibelTenths = -904.0;
constexpr double FullDecibelTenths = 0.0;

// The rise ends when the loudness is closer to full than this.
constexpr double AttackEndDistance = 0.03125;

constexpr std::uint8_t InstantParameter = 127;
constexpr std::uint8_t FastestButOneParameter = 126;
constexpr std::uint8_t SlowRangeEnd = 50;

// Attacks from 0 to 108 lose 255 - attack parts of 256 of the distance to full loudness for every 5 milliseconds.
// The faster attacks follow this list, which is read from the fastest to the slowest.
constexpr std::uint8_t FastAttackStart = 109;
constexpr std::array<std::uint8_t, 19> FastAttackRates = {0, 1, 5, 14, 26, 38, 51, 63, 73, 84, 92, 100, 109, 116, 123, 127, 132, 137, 143};
constexpr double MillisecondsPerStep = 5.0;
constexpr double StepFraction = 256.0;

// Sustain levels below this number of tenths of a decibel are not told apart.
constexpr int LowestSustainParameterWithFormula = 3;
constexpr double SilentSustainLevel = -723.0;

}

double AttackFactor(std::uint8_t attack)
{
    int rate = 0;
    if (attack < FastAttackStart)
        rate = 255 - attack;
    else
        rate = FastAttackRates[127 - attack];

    return std::pow(double(rate) / StepFraction, 1.0 / MillisecondsPerStep);
}

// The sustain parameter is the square of the volume: 127 is full, and each halving of the number is -12 dB.
double SustainLevel(std::uint8_t sustain)
{
    if (sustain < LowestSustainParameterWithFormula)
        return SilentSustainLevel + sustain;

    return std::round(400.0 * std::log10(double(sustain) / 127.0));
}

// How many tenths of a decibel the loudness falls in a millisecond.
double FallRate(std::uint8_t parameter)
{
    constexpr double InstantRate = 65535.0;
    constexpr double FastestButOneRate = 24.0;
    constexpr double SlowRangeStep = 1.0 / 128.0;
    constexpr double FastRangeNumerator = 60.0;
    constexpr double ParameterScale = 5.0;

    if (parameter == InstantParameter)
        return InstantRate;

    if (parameter == FastestButOneParameter)
        return FastestButOneRate;

    if (parameter < SlowRangeEnd)
        return (2 * parameter + 1) * SlowRangeStep / ParameterScale;

    return FastRangeNumerator / (FastestButOneParameter - parameter) / ParameterScale;
}

Envelope::Envelope(std::uint8_t attack, std::uint8_t decay, std::uint8_t sustain, std::uint8_t release)
    : stage(Stage::Attack)
    , decibelTenths(SilentDecibelTenths)
    , attackFactor(AttackFactor(attack))
    , decayRate(FallRate(decay))
    , sustainLevel(SustainLevel(sustain))
    , releaseRate(FallRate(release))
{
}

void Envelope::StartRelease()
{
    stage = Stage::Release;
}

void Envelope::SetRelease(std::uint8_t release)
{
    releaseRate = FallRate(release);
}

void Envelope::Advance(int milliseconds)
{
    if (stage == Stage::Attack)
    {
        for (int millisecond = 0; millisecond < milliseconds; ++millisecond)
        {
            decibelTenths *= attackFactor;
            if (decibelTenths > -AttackEndDistance)
            {
                decibelTenths = FullDecibelTenths;
                stage = Stage::Decay;
            }
        }
        return;
    }

    if (stage == Stage::Decay)
    {
        decibelTenths -= decayRate * milliseconds;
        if (decibelTenths <= sustainLevel)
        {
            decibelTenths = sustainLevel;
            stage = Stage::Sustain;
        }
        return;
    }

    if (stage == Stage::Release)
        decibelTenths -= releaseRate * milliseconds;
}

float Envelope::Gain() const
{
    // An attack that is over at once is at full loudness from the first moment.
    if (stage == Stage::Attack && attackFactor == 0.0)
        return 1.0f;

    if (decibelTenths <= SilentDecibelTenths)
        return 0.0f;

    constexpr double TenthsPerDecibel = 10.0;
    constexpr double DecibelsPerFactorOfTen = 20.0;
    double decibels = decibelTenths / TenthsPerDecibel;
    return static_cast<float>(std::pow(10.0, decibels / DecibelsPerFactorOfTen));
}

bool Envelope::IsFinished() const
{
    return stage == Stage::Release && decibelTenths <= SilentDecibelTenths;
}

}
