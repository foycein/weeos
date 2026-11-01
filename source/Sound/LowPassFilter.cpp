#include "Sound/LowPassFilter.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Sound
{

namespace
{

// The AX library works at this sample rate.
constexpr double LibrarySampleRate = 32000.0;

// The cutoff in hertz is 32 000 times two to the power of ten times (the value minus 1).
constexpr double OctavesForTheWholeValue = 10.0;
constexpr double CutoffForTheOpenValue = 32000.0;

// From this cutoff on, the library does not filter.
constexpr int LowestCutoffThatIsNotFiltered = 16000;

constexpr float NumbersPerStepOfTheValue = 64.0f;

// The coefficients of the library are whole numbers with 15 bits behind the point.
constexpr float CoefficientOne = 32768.0f;
constexpr int CoefficientAllBitsSet = 0x7FFF;

struct LibraryCoefficients
{
    int input;
    int feedback;
};

LibraryCoefficients LibraryCoefficientsFor(int cutoffHertz)
{
    float angle = 2.0f * std::numbers::pi_v<float> * float(cutoffHertz) / float(LibrarySampleRate);
    float distance = 2.0f - float(std::cos(double(angle)));
    float root = float(std::sqrt(double(distance * distance - 1.0f)));

    // The original makes a 16 bit whole number of this, so a value of the full 32768 would wrap around. The cutoffs it is used for stay below.
    int feedback = static_cast<std::int16_t>(static_cast<int>(CoefficientOne * -(root - distance)));
    return {CoefficientAllBitsSet - feedback, feedback};
}

}

float LowPassValueFromCutoffNumber(std::uint8_t number)
{
    return std::clamp(float(number) / NumbersPerStepOfTheValue, 0.0f, OpenLowPass);
}

int LowPassCutoffHertz(float value)
{
    double clampedValue = std::clamp(double(value), 0.0, double(OpenLowPass));
    return static_cast<int>(CutoffForTheOpenValue * std::pow(2.0, OctavesForTheWholeValue * (clampedValue - 1.0)));
}

std::optional<LowPassCoefficients> LowPassCoefficientsFor(float value, double sampleRate)
{
    int cutoffHertz = LowPassCutoffHertz(value);
    if (cutoffHertz >= LowestCutoffThatIsNotFiltered)
        return std::nullopt;

    LibraryCoefficients library = LibraryCoefficientsFor(cutoffHertz);
    float libraryFeedback = float(library.feedback) / CoefficientOne;
    float libraryDirectCurrentGain = float(library.input) / float(CoefficientOne - float(library.feedback));

    // A filter at another rate keeps the time that its output needs to follow its input.
    float feedback = float(std::pow(double(libraryFeedback), LibrarySampleRate / sampleRate));
    return LowPassCoefficients{(1.0f - feedback) * libraryDirectCurrentGain, feedback};
}

void LowPassFilter::SetValue(float newValue, double newSampleRate)
{
    if (newValue == value && newSampleRate == sampleRate)
        return;

    value = newValue;
    sampleRate = newSampleRate;

    std::optional<LowPassCoefficients> newCoefficients = LowPassCoefficientsFor(value, sampleRate);
    if (!coefficients && newCoefficients)
        previousOutput = 0.0f;

    coefficients = newCoefficients;
}

float LowPassFilter::Process(float sample)
{
    if (!coefficients)
        return sample;

    previousOutput = coefficients->input * sample + coefficients->feedback * previousOutput;
    return previousOutput;
}

}
