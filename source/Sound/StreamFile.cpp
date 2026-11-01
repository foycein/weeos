#include "Sound/StreamFile.h"

#include "Sound/AdpcmFrame.h"
#include "Sound/WaveInfo.h"

#include "Formats/ByteReader.h"

#include <format>

namespace Sound
{

namespace
{

constexpr std::string_view DataDescription = "Stream file";
constexpr std::size_t TagSize = 4;
constexpr std::string_view FileTag = "BNS ";
constexpr std::string_view InfoTag = "INFO";
constexpr std::string_view DataTag = "DATA";
constexpr std::uint32_t ByteOrderAndVersion = 0xFEFF0100;
constexpr std::size_t SectionHeaderSize = 8;
constexpr std::uint8_t DspAdpcmCodec = 0;
constexpr std::uint8_t MaximumChannelCount = 2;

struct SectionPlace
{
    std::uint32_t offset;
    std::uint32_t size;
};

struct StreamHeader
{
    SectionPlace info;
    SectionPlace data;
};

StreamHeader ReadStreamHeader(Formats::ByteReader& reader)
{
    std::string tag = reader.ReadText(TagSize);
    if (tag != FileTag)
        reader.Fail(std::format("the tag {} is not {}", tag, FileTag));

    std::uint32_t byteOrderAndVersion = reader.ReadUint32();
    if (byteOrderAndVersion != ByteOrderAndVersion)
        reader.Fail(std::format("the byte order and version {:#010x} are not known", byteOrderAndVersion));

    std::uint32_t fileSize = reader.ReadUint32();
    if (fileSize != reader.Size())
        reader.Fail(std::format("the header says the file has {} bytes, but it has {}", fileSize, reader.Size()));

    reader.ReadUint16();
    reader.ReadUint16();

    StreamHeader header;
    header.info = {reader.ReadUint32(), reader.ReadUint32()};
    header.data = {reader.ReadUint32(), reader.ReadUint32()};
    return header;
}

// The body of a section, after its tag and size.
Formats::ByteView ReadSectionBody(Formats::ByteView bytes, SectionPlace place, std::string_view expectedTag)
{
    Formats::ByteReader reader(bytes, DataDescription);
    reader.MoveTo(place.offset);
    Formats::ByteView section = reader.ReadBytes(place.size);

    Formats::ByteReader sectionReader(section, DataDescription);
    std::string tag = sectionReader.ReadText(TagSize);
    if (tag != expectedTag)
        sectionReader.Fail(std::format("the section {} was expected, but the section {} is there", expectedTag, tag));

    return section.subspan(SectionHeaderSize);
}

struct StreamChannel
{
    std::uint32_t dataOffset;
    AdpcmParameters adpcm;
};

// Every channel has a record of three words (where its samples start in the data, where its ADPCM parameters are, and an unused word);
// a table of offsets points to the records. All offsets count from the start of the INFO body.
StreamChannel ReadStreamChannel(Formats::ByteView infoBody, std::uint32_t channelTableOffset, std::uint8_t channelNumber)
{
    Formats::ByteReader reader(infoBody, DataDescription);
    reader.MoveTo(channelTableOffset + channelNumber * sizeof(std::uint32_t));
    std::uint32_t recordOffset = reader.ReadUint32();

    reader.MoveTo(recordOffset);
    StreamChannel channel;
    channel.dataOffset = reader.ReadUint32();
    std::uint32_t adpcmOffset = reader.ReadUint32();

    reader.MoveTo(adpcmOffset);
    channel.adpcm = ReadAdpcmParameters(reader);
    return channel;
}

}

DecodedWave ReadStreamFile(Formats::ByteView bytes)
{
    Formats::ByteReader reader(bytes, DataDescription);
    StreamHeader header = ReadStreamHeader(reader);
    Formats::ByteView infoBody = ReadSectionBody(bytes, header.info, InfoTag);
    Formats::ByteView dataBody = ReadSectionBody(bytes, header.data, DataTag);

    Formats::ByteReader infoReader(infoBody, DataDescription);
    std::uint8_t codec = infoReader.ReadByte();
    if (codec != DspAdpcmCodec)
        infoReader.Fail(std::format("the codec {} is not DSP-ADPCM", codec));

    DecodedWave wave;
    wave.loops = infoReader.ReadByte() != 0;
    std::uint8_t channelCount = infoReader.ReadByte();
    if (channelCount == 0 || channelCount > MaximumChannelCount)
        infoReader.Fail(std::format("the stream has {} channels", channelCount));

    infoReader.ReadByte();
    wave.sampleRate = infoReader.ReadUint16();
    infoReader.ReadUint16();
    wave.loopStartSample = infoReader.ReadUint32();
    std::uint32_t sampleCount = infoReader.ReadUint32();
    std::uint32_t channelTableOffset = infoReader.ReadUint32();

    for (std::uint8_t channelNumber = 0; channelNumber < channelCount; ++channelNumber)
    {
        StreamChannel channel = ReadStreamChannel(infoBody, channelTableOffset, channelNumber);
        Formats::ByteReader dataReader(dataBody, DataDescription);
        dataReader.MoveTo(channel.dataOffset);
        Formats::ByteView frames = dataReader.ReadBytes(AdpcmByteCount(sampleCount));
        wave.channels.push_back(DecodeAdpcm(frames, channel.adpcm, sampleCount));
    }
    return wave;
}

}
