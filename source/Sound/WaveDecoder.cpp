#include "Sound/WaveDecoder.h"

#include "Sound/AdpcmFrame.h"

#include "Formats/ByteReader.h"
#include "Formats/FormatError.h"

#include <algorithm>
#include <format>
#include <limits>

namespace Sound
{

namespace
{

constexpr std::int32_t FullScale = 32768;
constexpr int PredictorBitCount = 4;
constexpr int MaximumPredictor = 7;
constexpr int ScaleMask = 0x0F;
constexpr int NibbleSignBit = 8;
constexpr int NibbleMask = 0x0F;

// The corrections are fixed point numbers with 11 fractional bits. Half of the last bit is added before it is cut off, to round.
constexpr int FractionBitCount = 11;
constexpr std::int64_t Rounding = std::int64_t(1) << (FractionBitCount - 1);

float ToFloat(std::int32_t sample)
{
    return static_cast<float>(sample) / FullScale;
}

std::int32_t LimitTo16Bit(std::int64_t sample)
{
    constexpr std::int64_t Lowest = std::numeric_limits<std::int16_t>::min();
    constexpr std::int64_t Highest = std::numeric_limits<std::int16_t>::max();
    return static_cast<std::int32_t>(std::clamp(sample, Lowest, Highest));
}

// The four bits of a frame hold a number from -8 to 7.
std::int32_t SignedNibble(int nibble)
{
    return (nibble ^ NibbleSignBit) - NibbleSignBit;
}

struct FrameHeader
{
    int coefficient1;
    int coefficient2;
    int scale;
};

FrameHeader ReadFrameHeader(Formats::Byte headerByte, const AdpcmParameters& parameters)
{
    int predictor = headerByte >> PredictorBitCount;
    if (predictor > MaximumPredictor)
        throw Formats::FormatError(std::format("Wave: the predictor {} of an ADPCM frame does not exist", predictor));

    FrameHeader header;
    header.coefficient1 = parameters.coefficients[2 * predictor];
    header.coefficient2 = parameters.coefficients[2 * predictor + 1];
    header.scale = headerByte & ScaleMask;
    return header;
}

void CheckSize(Formats::ByteView bytes, std::uint32_t neededSize, std::string_view encodingName)
{
    if (bytes.size() < neededSize)
    {
        throw Formats::FormatError(std::format("Wave: {} samples need {} bytes, but there are {}", encodingName, neededSize, bytes.size()));
    }
}

// The new sample is the prediction from the two samples before it, plus the correction that the frame holds.
std::int32_t DecodeSample(const FrameHeader& header, int nibble, std::int32_t newerSample, std::int32_t olderSample)
{
    std::int64_t correction = (std::int64_t(SignedNibble(nibble)) * (std::int64_t(1) << header.scale)) << FractionBitCount;
    std::int64_t prediction = std::int64_t(header.coefficient1) * newerSample + std::int64_t(header.coefficient2) * olderSample;
    return LimitTo16Bit((correction + prediction + Rounding) >> FractionBitCount);
}

}

std::size_t DecodedWave::SampleCount() const
{
    return channels.empty() ? 0 : channels[0].size();
}

std::vector<float> DecodeAdpcm(Formats::ByteView frames, const AdpcmParameters& parameters, std::uint32_t sampleCount)
{
    CheckSize(frames, AdpcmByteCount(sampleCount), "DSP-ADPCM");

    std::vector<float> samples;
    samples.reserve(sampleCount);

    std::int32_t newerSample = parameters.initialHistory1;
    std::int32_t olderSample = parameters.initialHistory2;

    for (std::uint32_t frameNumber = 0; frameNumber < AdpcmFrameCount(sampleCount); ++frameNumber)
    {
        std::size_t frameStart = std::size_t(frameNumber) * AdpcmBytesPerFrame;
        FrameHeader header = ReadFrameHeader(frames[frameStart], parameters);

        for (std::uint32_t sampleInFrame = 0; sampleInFrame < AdpcmSamplesPerFrame && samples.size() < sampleCount; ++sampleInFrame)
        {
            Formats::Byte pairOfNibbles = frames[frameStart + 1 + sampleInFrame / 2];
            int nibble = (sampleInFrame % 2 == 0) ? (pairOfNibbles >> 4) : (pairOfNibbles & NibbleMask);

            std::int32_t sample = DecodeSample(header, nibble, newerSample, olderSample);
            olderSample = newerSample;
            newerSample = sample;
            samples.push_back(ToFloat(sample));
        }
    }
    return samples;
}

std::vector<float> DecodePcm16(Formats::ByteView bytes, std::uint32_t sampleCount)
{
    CheckSize(bytes, sampleCount * sizeof(std::int16_t), "16 bit");

    Formats::ByteReader reader(bytes, "Wave");
    std::vector<float> samples;
    samples.reserve(sampleCount);
    for (std::uint32_t sampleNumber = 0; sampleNumber < sampleCount; ++sampleNumber)
        samples.push_back(ToFloat(reader.ReadInt16()));

    return samples;
}

std::vector<float> DecodePcm8(Formats::ByteView bytes, std::uint32_t sampleCount)
{
    constexpr std::int32_t BitsToMakeItSixteen = 8;

    CheckSize(bytes, sampleCount, "8 bit");

    std::vector<float> samples;
    samples.reserve(sampleCount);
    for (std::uint32_t sampleNumber = 0; sampleNumber < sampleCount; ++sampleNumber)
    {
        std::int32_t signedSample = static_cast<std::int8_t>(bytes[sampleNumber]);
        samples.push_back(ToFloat(signedSample * (1 << BitsToMakeItSixteen)));
    }
    return samples;
}

DecodedWave DecodeWave(const WaveInfo& info, Formats::ByteView waveData)
{
    DecodedWave wave;
    wave.sampleRate = info.sampleRate;
    wave.loops = info.loops;
    wave.loopStartSample = info.loops ? info.LoopStartSample() : 0;

    std::uint32_t sampleCount = info.SampleCount();
    std::uint32_t byteCount = info.ChannelByteCount();
    for (const WaveChannel& channel : info.channels)
    {
        std::uint64_t channelStart = std::uint64_t(info.dataLocation) + channel.sampleDataOffset;
        if (!Formats::IsRangeInside(channelStart, byteCount, waveData.size()))
            throw Formats::FormatError("Wave: the samples of a channel lie outside the wave data");

        Formats::ByteView channelBytes = waveData.subspan(static_cast<std::size_t>(channelStart), byteCount);
        if (info.encoding == WaveEncoding::DspAdpcm)
            wave.channels.push_back(DecodeAdpcm(channelBytes, channel.adpcm, sampleCount));
        else if (info.encoding == WaveEncoding::Pcm16)
            wave.channels.push_back(DecodePcm16(channelBytes, sampleCount));
        else
            wave.channels.push_back(DecodePcm8(channelBytes, sampleCount));
    }
    return wave;
}

}
