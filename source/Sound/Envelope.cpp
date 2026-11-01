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

// Every 5 milliseconds an attack from 0 to 108 multiplies the distance to full loudness by (255 - attack) / 256.
// The faster attacks follow this list, which is read from the fastest to the slowest.
constexpr int SlowestAttackRate = 255;
constexpr std::uint8_t FastAttackStart = 109;
constexpr std::array<std::uint8_t, 19> FastAttackRates = {0, 1, 5, 14, 26, 38, 51, 63, 73, 84, 92, 100, 109, 116, 123, 127, 132, 137, 143};
constexpr double MillisecondsPerStep = 5.0;
constexpr double StepFraction = 256.0;

// The sustain parameter is the square of the volume: 127 is full, and each halving of the number is -12 dB.
constexpr double FullSustainParameter = 127.0;
constexpr double SustainTenthsPerFactorOfTen = 400.0;

// The sustain parameters below this one do not follow the formula: they lie just above silence, a tenth of a decibel apart.
constexpr int LowestSustainParameterWithFormula = 3;
constexpr double SilentSustainLevel = -723.0;

}

double AttackFactor(std::uint8_t attack)
{
    int rate = 0;
    if (attack < FastAttackStart)
        rate = SlowestAttackRate - attack;
    else
        rate = FastAttackRates[InstantParameter - attack];

    return std::pow(double(rate) / StepFraction, 1.0 / MillisecondsPerStep);
}

double SustainLevel(std::uint8_t sustain)
{
    if (sustain < LowestSustainParameterWithFormula)
        return SilentSustainLevel + sustain;

    return std::round(SustainTenthsPerFactorOfTen * std::log10(double(sustain) / FullSustainParameter));
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
    {
        double oddNumber = 2.0 * parameter + 1.0;
        return oddNumber * SlowRangeStep / ParameterScale;
    }

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

void Envelope::SetAttack(std::uint8_t attack)
{
    attackFactor = AttackFactor(attack);
}

void Envelope::SetDecay(std::uint8_t decay)
{
    decayRate = FallRate(decay);
}

void Envelope::SetSustain(std::uint8_t sustain)
{
    sustainLevel = SustainLevel(sustain);
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
