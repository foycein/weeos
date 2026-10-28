#pragma once

#include "Formats/ByteReader.h"

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

namespace Sound
{

enum class WaveEncoding : std::uint8_t
{
    Pcm8 = 0,
    Pcm16 = 1,
    DspAdpcm = 2,
};

constexpr std::size_t AdpcmCoefficientCount = 16;

// What the decoder needs to know about the compressed samples of one channel.
struct AdpcmParameters
{
    // Two coefficients for each of the eight predictors.
    std::array<std::int16_t, AdpcmCoefficientCount> coefficients;

    // The header byte of the first frame and the two samples that come before the first one.
    std::uint16_t initialPredictorAndScale;
    std::int16_t initialHistory1;
    std::int16_t initialHistory2;

    // The same for the frame at which a looping wave starts its loop.
    std::uint16_t loopPredictorAndScale;
    std::int16_t loopHistory1;
    std::int16_t loopHistory2;
};

struct WaveChannel
{
    // Where the samples of this channel start, counted from the start of the samples of the wave.
    std::uint32_t sampleDataOffset;

    // Only for DSP-ADPCM.
    AdpcmParameters adpcm;
};

// The description of one recorded wave, as the wave sound files and the banks store it (28 bytes plus a table of channels).
struct WaveInfo
{
    WaveEncoding encoding;
    bool loops;
    std::uint32_t sampleRate;

    // Where the loop starts and where the wave ends. For DSP-ADPCM these are nibble addresses
    // (the position of a half byte, counting the header of every frame), for the other encodings sample numbers.
    std::uint32_t loopStartAddress;
    std::uint32_t endAddress;

    // Where the samples of the wave begin, counted from the start of the wave data of the sound file.
    std::uint32_t dataLocation;

    std::vector<WaveChannel> channels;

    // The number of samples in each channel.
    std::uint32_t SampleCount() const;

    // The sample at which a looping wave goes back to. Only for looping waves.
    std::uint32_t LoopStartSample() const;

    // The size in bytes of the samples of one channel.
    std::uint32_t ChannelByteCount() const;
};

// Reads the wave info that starts at waveInfoOffset. The offsets inside it count from there.
WaveInfo ReadWaveInfo(Formats::ByteView sectionBytes, std::size_t waveInfoOffset, std::string_view dataDescription);

}
