#include "Sound/AudioFile.h"

#include "Formats/FormatError.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <optional>
#include <string_view>

namespace Sound
{

namespace
{

constexpr std::size_t TagSize = 4;
constexpr std::uint16_t PcmFormatOfWave = 1;
constexpr float SixteenBitScale = 32768.0f;
constexpr float EightBitScale = 128.0f;

// 8 bit samples of a WAV file have no sign: 128 is silence. Those of an AIFF file have a sign.
constexpr int SilenceOfUnsignedByte = 128;

struct SampleLayout
{
    std::uint16_t channelCount = 0;
    std::uint16_t bitsPerSample = 0;
    std::uint32_t sampleRate = 0;
};

std::string_view TagAt(Formats::ByteView bytes, std::size_t position)
{
    if (position + TagSize > bytes.size())
        throw Formats::FormatError("Audio file: the file ends inside a tag");

    return std::string_view(reinterpret_cast<const char*>(bytes.data() + position), TagSize);
}

std::uint32_t LittleEndianNumber(Formats::ByteView bytes, std::size_t position, std::size_t byteCount)
{
    if (position + byteCount > bytes.size())
        throw Formats::FormatError("Audio file: the file ends inside a number");

    std::uint32_t number = 0;
    for (std::size_t index = 0; index < byteCount; ++index)
        number |= std::uint32_t(bytes[position + index]) << (8 * index);
    return number;
}

std::uint32_t BigEndianNumber(Formats::ByteView bytes, std::size_t position, std::size_t byteCount)
{
    if (position + byteCount > bytes.size())
        throw Formats::FormatError("Audio file: the file ends inside a number");

    std::uint32_t number = 0;
    for (std::size_t index = 0; index < byteCount; ++index)
        number = (number << 8) | bytes[position + index];
    return number;
}

// AIFF keeps its sample rate as an 80 bit floating point number: a sign and 15 bits of exponent, then 64 bits of mantissa.
std::uint32_t ExtendedNumber(Formats::ByteView bytes, std::size_t position)
{
    constexpr int ExponentBias = 16383;
    constexpr int MantissaBits = 63;
    constexpr std::uint32_t ExponentMask = 0x7FFF;
    constexpr int BitsOfTheLowerHalf = 32;

    std::uint32_t exponentField = BigEndianNumber(bytes, position, 2) & ExponentMask;
    double upperHalf = std::ldexp(double(BigEndianNumber(bytes, position + 2, 4)), BitsOfTheLowerHalf);
    double mantissa = upperHalf + double(BigEndianNumber(bytes, position + 6, 4));
    double value = std::ldexp(mantissa, int(exponentField) - ExponentBias - MantissaBits);
    return std::uint32_t(std::lround(value));
}

void CheckLayout(const SampleLayout& layout)
{
    bool hasGoodChannels = layout.channelCount == 1 || layout.channelCount == 2;
    bool hasGoodBits = layout.bitsPerSample == 8 || layout.bitsPerSample == 16;
    if (!hasGoodChannels || !hasGoodBits || layout.sampleRate == 0)
    {
        throw Formats::FormatError(std::format("Audio file: {} channels of {} bits at {} Hz cannot be played", layout.channelCount,
            layout.bitsPerSample, layout.sampleRate));
    }
}

float SampleOfWave(Formats::ByteView samples, std::size_t position, std::uint16_t bitsPerSample)
{
    if (bitsPerSample == 8)
        return float(int(samples[position]) - SilenceOfUnsignedByte) / EightBitScale;

    std::int16_t sample = static_cast<std::int16_t>(LittleEndianNumber(samples, position, 2));
    return float(sample) / SixteenBitScale;
}

float SampleOfAiff(Formats::ByteView samples, std::size_t position, std::uint16_t bitsPerSample)
{
    if (bitsPerSample == 8)
        return float(static_cast<std::int8_t>(samples[position])) / EightBitScale;

    std::int16_t sample = static_cast<std::int16_t>(BigEndianNumber(samples, position, 2));
    return float(sample) / SixteenBitScale;
}

// The samples of the channels come one after the other, frame by frame.
template <typename ReadSample>
DecodedWave Deinterleave(Formats::ByteView samples, const SampleLayout& layout, ReadSample readSample)
{
    std::size_t bytesPerSample = layout.bitsPerSample / 8;
    std::size_t bytesPerFrame = bytesPerSample * layout.channelCount;
    std::size_t frameCount = samples.size() / bytesPerFrame;

    DecodedWave wave{layout.sampleRate, false, 0, std::vector<std::vector<float>>(layout.channelCount)};
    for (std::vector<float>& channel : wave.channels)
        channel.reserve(frameCount);

    for (std::size_t frame = 0; frame < frameCount; ++frame)
    {
        for (std::size_t channel = 0; channel < layout.channelCount; ++channel)
            wave.channels[channel].push_back(readSample(samples, frame * bytesPerFrame + channel * bytesPerSample, layout.bitsPerSample));
    }
    return wave;
}

// RIFF, its size, WAVE, then chunks: "fmt " with the layout and "data" with the samples. A chunk of an odd size is followed by a pad byte.
DecodedWave ReadWave(Formats::ByteView file)
{
    constexpr std::size_t FirstChunk = 12;
    constexpr std::size_t ChunkHeaderSize = 8;

    if (TagAt(file, 8) != "WAVE")
        throw Formats::FormatError("Audio file: a RIFF file that is not WAVE");

    SampleLayout layout;
    std::optional<Formats::ByteView> samples;
    std::size_t position = FirstChunk;
    while (position + ChunkHeaderSize <= file.size())
    {
        std::string_view tag = TagAt(file, position);
        std::uint32_t chunkSize = LittleEndianNumber(file, position + 4, 4);
        std::size_t body = position + ChunkHeaderSize;
        if (body + chunkSize > file.size())
            throw Formats::FormatError("Audio file: a chunk goes past the end of the file");

        if (tag == "fmt ")
        {
            if (LittleEndianNumber(file, body, 2) != PcmFormatOfWave)
                throw Formats::FormatError("Audio file: the WAV file is not PCM");

            layout.channelCount = std::uint16_t(LittleEndianNumber(file, body + 2, 2));
            layout.sampleRate = LittleEndianNumber(file, body + 4, 4);
            layout.bitsPerSample = std::uint16_t(LittleEndianNumber(file, body + 14, 2));
        }

        if (tag == "data")
            samples = file.subspan(body, chunkSize);

        position = body + chunkSize + chunkSize % 2;
    }

    if (!samples)
        throw Formats::FormatError("Audio file: the WAV file has no samples");

    CheckLayout(layout);
    return Deinterleave(*samples, layout, SampleOfWave);
}

// FORM, its size, AIFF, then chunks: "COMM" with the layout and "SSND" with an offset, a block size and the samples.
DecodedWave ReadAiff(Formats::ByteView file)
{
    constexpr std::size_t FirstChunk = 12;
    constexpr std::size_t ChunkHeaderSize = 8;
    constexpr std::size_t SoundDataHeaderSize = 8;

    if (TagAt(file, 8) != "AIFF")
        throw Formats::FormatError("Audio file: a FORM file that is not AIFF");

    SampleLayout layout;
    std::optional<Formats::ByteView> samples;
    std::size_t position = FirstChunk;
    while (position + ChunkHeaderSize <= file.size())
    {
        std::string_view tag = TagAt(file, position);
        std::uint32_t chunkSize = BigEndianNumber(file, position + 4, 4);
        std::size_t body = position + ChunkHeaderSize;
        if (body + chunkSize > file.size())
            throw Formats::FormatError("Audio file: a chunk goes past the end of the file");

        if (tag == "COMM")
        {
            layout.channelCount = std::uint16_t(BigEndianNumber(file, body, 2));
            layout.bitsPerSample = std::uint16_t(BigEndianNumber(file, body + 6, 2));
            layout.sampleRate = ExtendedNumber(file, body + 8);
        }

        if (tag == "SSND" && chunkSize >= SoundDataHeaderSize)
        {
            std::uint32_t offset = BigEndianNumber(file, body, 4);
            std::size_t first = std::min<std::size_t>(SoundDataHeaderSize + offset, chunkSize);
            samples = file.subspan(body + first, chunkSize - first);
        }

        position = body + chunkSize + chunkSize % 2;
    }

    if (!samples)
        throw Formats::FormatError("Audio file: the AIFF file has no samples");

    CheckLayout(layout);
    return Deinterleave(*samples, layout, SampleOfAiff);
}

}

DecodedWave ReadAudioFile(Formats::ByteView file)
{
    std::string_view tag = TagAt(file, 0);
    if (tag == "RIFF")
        return ReadWave(file);

    if (tag == "FORM")
        return ReadAiff(file);

    throw Formats::FormatError("Audio file: neither a WAV nor an AIFF file");
}

}
