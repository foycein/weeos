#include "Input/SpeakerAdpcm.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <limits>

namespace Input
{

namespace
{

// The rules of Yamaha's 4 bit ADPCM. A code is a size of 0 to 7 and a sign bit; the size says how many eighths of the step
// size the sample moves (1, 3, 5 ... 15 eighths), and also how the step size grows or shrinks for the next sample (in 256ths).
constexpr std::uint8_t SignBit = 0x08;
constexpr int LargestSize = 7;
constexpr int SizeMask = 0x07;
constexpr auto StepEighths = std::to_array<int>({1, 3, 5, 7, 9, 11, 13, 15});
constexpr auto StepChangeIn256ths = std::to_array<int>({230, 230, 230, 230, 307, 409, 512, 614});
constexpr int LargestStepSize = 24567;
constexpr int EighthsPerStep = 8;
constexpr int QuartersPerStep = 4;
constexpr int StepChangeShift = 8;
constexpr int HighHalfShift = 4;

// The decoder of the speaker does the same: the encoder must follow it exactly.
void Advance(std::uint8_t code, int& predictedSample, int& stepSize)
{
    int size = code & SizeMask;
    int move = stepSize * StepEighths[size] / EighthsPerStep;
    if ((code & SignBit) != 0)
        move = -move;

    predictedSample += move;
    predictedSample = std::clamp<int>(predictedSample, std::numeric_limits<std::int16_t>::min(), std::numeric_limits<std::int16_t>::max());

    stepSize = (stepSize * StepChangeIn256ths[size]) >> StepChangeShift;
    stepSize = std::clamp(stepSize, StartingAdpcmStepSize, LargestStepSize);
}

}

std::uint8_t SpeakerAdpcmEncoder::EncodeSample(std::int16_t sample)
{
    int difference = sample - predictedSample;
    int size = std::min(LargestSize, std::abs(difference) * QuartersPerStep / stepSize);

    std::uint8_t code = static_cast<std::uint8_t>(size);
    if (difference < 0)
        code |= SignBit;

    Advance(code, predictedSample, stepSize);
    return code;
}

std::vector<std::uint8_t> SpeakerAdpcmEncoder::Encode(std::span<const std::int16_t> samples)
{
    std::vector<std::uint8_t> bytes;
    for (std::size_t index = 0; index < samples.size(); index += 2)
    {
        std::int16_t secondSample = index + 1 < samples.size() ? samples[index + 1] : 0;
        std::uint8_t firstCode = EncodeSample(samples[index]);
        std::uint8_t secondCode = EncodeSample(secondSample);
        bytes.push_back(static_cast<std::uint8_t>((firstCode << HighHalfShift) | secondCode));
    }

    return bytes;
}

std::int16_t SpeakerAdpcmDecoder::DecodeSample(std::uint8_t code)
{
    Advance(code, predictedSample, stepSize);
    return static_cast<std::int16_t>(predictedSample);
}

}
