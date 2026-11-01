#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace Input
{

// The step size of the ADPCM at the start, which is also its smallest.
constexpr int StartingAdpcmStepSize = 127;

// Packs sound for the speaker of a Wii Remote: 4 bit Yamaha ADPCM, two samples in a byte, the first one in the high half
// (WiiBrew, Wiimote, Speaker). The encoder remembers where it is, because the speaker's decoder runs on from packet to packet.
class SpeakerAdpcmEncoder
{
public:
    // An even number of samples gives half as many bytes. A last odd sample is paired with silence.
    std::vector<std::uint8_t> Encode(std::span<const std::int16_t> samples);

    // The 4 bit code of one sample.
    std::uint8_t EncodeSample(std::int16_t sample);

private:
    int predictedSample = 0;
    int stepSize = StartingAdpcmStepSize;
};

// What the speaker's decoder makes of the codes: the same steps the other way, to check the encoder.
class SpeakerAdpcmDecoder
{
public:
    std::int16_t DecodeSample(std::uint8_t code);

private:
    int predictedSample = 0;
    int stepSize = StartingAdpcmStepSize;
};

}
