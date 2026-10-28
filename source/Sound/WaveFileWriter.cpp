#include "Sound/WaveFileWriter.h"

#include <bit>
#include <format>
#include <fstream>
#include <stdexcept>
#include <string_view>

namespace Sound
{

namespace
{

constexpr std::uint16_t IeeeFloatFormat = 3;
constexpr std::uint32_t FormatChunkSize = 18;
constexpr std::uint32_t FactChunkSize = 4;
constexpr std::uint16_t BitsPerSample = 32;
constexpr std::uint32_t ChunkHeaderSize = 8;
constexpr std::uint32_t WaveTagSize = 4;

// WAV files store numbers with the least significant byte first.
void AppendNumber(Formats::Bytes& bytes, std::uint64_t number, std::size_t byteCount)
{
    for (std::size_t byteNumber = 0; byteNumber < byteCount; ++byteNumber)
        bytes.push_back(static_cast<Formats::Byte>(number >> (Formats::BitsPerByte * byteNumber)));
}

void AppendTag(Formats::Bytes& bytes, std::string_view tag)
{
    bytes.insert(bytes.end(), tag.begin(), tag.end());
}

}

Formats::Bytes MakeFloatWaveFileBytes(std::uint32_t sampleRate, std::uint16_t channelCount, std::span<const float> interleavedSamples)
{
    std::uint32_t sampleFrameCount = static_cast<std::uint32_t>(interleavedSamples.size() / channelCount);
    std::uint16_t bytesPerFrame = static_cast<std::uint16_t>(channelCount * sizeof(float));
    std::uint32_t dataSize = static_cast<std::uint32_t>(interleavedSamples.size() * sizeof(float));
    std::uint32_t formatChunkTotalSize = ChunkHeaderSize + FormatChunkSize;
    std::uint32_t factChunkTotalSize = ChunkHeaderSize + FactChunkSize;
    std::uint32_t dataChunkTotalSize = ChunkHeaderSize + dataSize;
    std::uint32_t fileSizeAfterRiffHeader = WaveTagSize + formatChunkTotalSize + factChunkTotalSize + dataChunkTotalSize;

    Formats::Bytes bytes;
    AppendTag(bytes, "RIFF");
    AppendNumber(bytes, fileSizeAfterRiffHeader, sizeof(std::uint32_t));
    AppendTag(bytes, "WAVE");

    AppendTag(bytes, "fmt ");
    AppendNumber(bytes, FormatChunkSize, sizeof(std::uint32_t));
    AppendNumber(bytes, IeeeFloatFormat, sizeof(std::uint16_t));
    AppendNumber(bytes, channelCount, sizeof(std::uint16_t));
    AppendNumber(bytes, sampleRate, sizeof(std::uint32_t));
    AppendNumber(bytes, std::uint64_t(sampleRate) * bytesPerFrame, sizeof(std::uint32_t));
    AppendNumber(bytes, bytesPerFrame, sizeof(std::uint16_t));
    AppendNumber(bytes, BitsPerSample, sizeof(std::uint16_t));
    AppendNumber(bytes, 0, sizeof(std::uint16_t));

    AppendTag(bytes, "fact");
    AppendNumber(bytes, FactChunkSize, sizeof(std::uint32_t));
    AppendNumber(bytes, sampleFrameCount, sizeof(std::uint32_t));

    AppendTag(bytes, "data");
    AppendNumber(bytes, dataSize, sizeof(std::uint32_t));
    for (float sample : interleavedSamples)
        AppendNumber(bytes, std::bit_cast<std::uint32_t>(sample), sizeof(std::uint32_t));

    return bytes;
}

void WriteFloatWaveFile(const std::filesystem::path& path, std::uint32_t sampleRate, std::uint16_t channelCount,
    std::span<const float> interleavedSamples)
{
    Formats::Bytes bytes = MakeFloatWaveFileBytes(sampleRate, channelCount, interleavedSamples);

    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!file)
        throw std::runtime_error(std::format("Cannot write the file {}", path.string()));
}

}
