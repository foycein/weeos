#include "Sound/StreamFile.h"

#include "Sound/AdpcmFrame.h"

#include "Formats/Alignment.h"
#include "Formats/ByteReader.h"
#include "Formats/FormatError.h"

#include <format>
#include <string_view>

namespace Sound
{

namespace
{

constexpr std::string_view Description = "Stream file";

// The stream starts with its tag, the byte order mark and the version, its size, the size of this header and the number of chunks,
// then the place and the size of the two chunks INFO and DATA. The places count from the start of the stream.
constexpr std::string_view StreamTag = "BNS ";
constexpr std::size_t FieldsBeforeChunkPlaces = 12;
constexpr std::string_view InfoTag = "INFO";
constexpr std::string_view DataTag = "DATA";
constexpr std::size_t ChunkSizeField = 4;

// What the information chunk holds before the table of the channels. The places inside the chunk count from behind its tag and size.
constexpr std::uint8_t AdpcmCodec = 0;
constexpr std::uint8_t MaximumChannelCount = 2;
constexpr std::size_t FillAfterChannelCount = 1;
constexpr std::size_t FillAfterSampleRate = 2;

void CheckTag(Formats::ByteReader& reader, std::string_view tag)
{
    std::string text = reader.ReadText(tag.size());
    if (text != tag)
        reader.Fail(std::format("the tag {} is not {}", text, tag));
}

// Where the samples and the ADPCM parameters of one channel are: the information chunk holds a table with the place of a record for every
// channel, and the record holds the place of the samples (in the data chunk) and of the parameters (in the information chunk).
struct StreamPlaces
{
    std::size_t infoStart;
    std::size_t channelTableStart;
    std::size_t dataStart;
};

std::vector<float> ReadChannel(Formats::ByteView stream, const StreamPlaces& places, std::uint32_t channelNumber, std::uint32_t sampleCount)
{
    Formats::ByteReader reader(stream, Description);
    reader.MoveTo(places.channelTableStart + sizeof(std::uint32_t) * channelNumber);
    std::uint32_t channelRecord = reader.ReadUint32();

    reader.MoveTo(places.infoStart + channelRecord);
    std::uint32_t samplesOffset = reader.ReadUint32();
    std::uint32_t adpcmOffset = reader.ReadUint32();

    reader.MoveTo(places.infoStart + adpcmOffset);
    AdpcmParameters parameters = ReadAdpcmParameters(reader);

    std::uint64_t channelStart = std::uint64_t(places.dataStart) + samplesOffset;
    std::uint32_t byteCount = AdpcmByteCount(sampleCount);
    if (!Formats::IsRangeInside(channelStart, byteCount, stream.size()))
        reader.Fail(std::format("the samples of channel {} lie behind the end of the file", channelNumber));

    return DecodeAdpcm(stream.subspan(static_cast<std::size_t>(channelStart), byteCount), parameters, sampleCount);
}

}

DecodedWave ReadStreamFile(Formats::ByteView stream)
{
    Formats::ByteReader reader(stream, Description);
    CheckTag(reader, StreamTag);
    reader.Skip(FieldsBeforeChunkPlaces);
    std::uint32_t infoPlace = reader.ReadUint32();
    reader.Skip(ChunkSizeField);
    std::uint32_t dataPlace = reader.ReadUint32();

    reader.MoveTo(dataPlace);
    CheckTag(reader, DataTag);
    reader.Skip(ChunkSizeField);
    std::size_t dataStart = reader.Position();

    reader.MoveTo(infoPlace);
    CheckTag(reader, InfoTag);
    reader.Skip(ChunkSizeField);
    std::size_t infoStart = reader.Position();

    std::uint8_t codec = reader.ReadByte();
    if (codec != AdpcmCodec)
        reader.Fail(std::format("the codec {} is not DSP-ADPCM", codec));

    DecodedWave wave;
    wave.loops = reader.ReadByte() != 0;
    std::uint8_t channelCount = reader.ReadByte();
    if (channelCount == 0 || channelCount > MaximumChannelCount)
        reader.Fail(std::format("{} channels are not one or two", channelCount));

    reader.Skip(FillAfterChannelCount);
    wave.sampleRate = reader.ReadUint16();
    reader.Skip(FillAfterSampleRate);
    std::uint32_t loopStart = reader.ReadUint32();
    std::uint32_t sampleCount = reader.ReadUint32();
    std::uint32_t channelTable = reader.ReadUint32();
    if (wave.loops && loopStart >= sampleCount)
        reader.Fail("the loop starts behind the end of the sound");

    wave.loopStartSample = wave.loops ? loopStart : 0;
    StreamPlaces places = {infoStart, infoStart + channelTable, dataStart};
    for (std::uint32_t channelNumber = 0; channelNumber < channelCount; ++channelNumber)
        wave.channels.push_back(ReadChannel(stream, places, channelNumber, sampleCount));

    return wave;
}

}
