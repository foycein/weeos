#pragma once

#include <cstdint>
#include <optional>

namespace Sound
{

// The low-pass filter of a voice takes the high tones away. Its value goes from 0, where only the lowest tones stay, to 1, where
// the voice is not filtered at all.
constexpr float OpenLowPass = 1.0f;

// The sequences set the filter with a number from 0 to 127. 64 and above do not filter.
float LowPassValueFromCutoffNumber(std::uint8_t number);

// The frequency in hertz above which a voice with this value gets quieter (0x8150f43c). It runs from 31 Hz for the value 0
// up to 32 000 Hz for the value 1, doubling for each tenth of the value.
int LowPassCutoffHertz(float value);

// The filter makes each output sample from the sample of the voice and the output sample before it:
// output = input * sample + feedback * previous output.
struct LowPassCoefficients
{
    float input;
    float feedback;
};

// The coefficients for the value at the sample rate of the device. They are those of the AX library at 32 000 Hz (0x81555724), moved to
// the rate of the device so that the cutoff stays the same. Nothing when the voice is not filtered: from 16 000 Hz up the library
// switches the filter off.
std::optional<LowPassCoefficients> LowPassCoefficientsFor(float value, double sampleRate);

// The filter of one channel of a voice.
class LowPassFilter
{
public:
    // Takes a new value from the next sample on. Nothing changes when the value is the one the filter already has.
    void SetValue(float newValue, double sampleRate);

    float Process(float sample);

private:
    float value = OpenLowPass;
    double sampleRate = 0.0;
    std::optional<LowPassCoefficients> coefficients;
    float previousOutput = 0.0f;
};

}
