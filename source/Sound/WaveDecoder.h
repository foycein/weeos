#pragma once

#include "Sound/WaveInfo.h"

#include "Formats/Bytes.h"

#include <cstdint>
#include <vector>

namespace Sound
{

// The samples of a wave as numbers between -1 and 1, decoded once and ready to be played.
struct DecodedWave
{
    std::uint32_t sampleRate;

    bool loops;

    // The sample at which a looping wave goes back to when it reaches its end.
    std::uint32_t loopStartSample;

    // One list of samples for each channel, all of the same length.
    std::vector<std::vector<float>> channels;

    std::size_t SampleCount() const;
};

// Decodes the DSP-ADPCM samples of one channel. The samples come in frames of 14 samples. The first sample of a frame
// is predicted from the two samples before it, and the 4 bit number in the frame corrects the prediction.
// Gives sampleCount samples as 16 bit numbers divided by 32768.
std::vector<float> DecodeAdpcm(Formats::ByteView frames, const AdpcmParameters& parameters, std::uint32_t sampleCount);

// The same for 16 bit samples that are stored with the most significant byte first, and for 8 bit samples.
std::vector<float> DecodePcm16(Formats::ByteView bytes, std::uint32_t sampleCount);
std::vector<float> DecodePcm8(Formats::ByteView bytes, std::uint32_t sampleCount);

// Decodes all channels of a wave. waveData is the wave data of the file the wave belongs to.
DecodedWave DecodeWave(const WaveInfo& info, Formats::ByteView waveData);

}
