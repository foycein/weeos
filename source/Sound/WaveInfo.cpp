#include "Sound/WaveInfo.h"

#include "Sound/AdpcmFrame.h"

#include <format>

namespace Sound
{

namespace
{

constexpr std::uint8_t OffsetLocationType = 0;
constexpr std::size_t UnusedAfterLocationTypeSize = 1;
constexpr std::size_t SampleRateHighByteBits = 16;
constexpr std::size_t VolumeFieldsSize = 4 * sizeof(std::uint32_t);
constexpr std::size_t UnusedAtEndOfChannelSize = sizeof(std::uint32_t);
constexpr std::uint8_t MaximumChannelCount = 2;

// The addresses of a DSP-ADPCM wave must point at samples and not at frame headers. A loop must lie inside the wave.
void CheckAddresses(Formats::ByteReader& reader, const WaveInfo& info)
{
    if (info.encoding == WaveEncoding::DspAdpcm && !IsAdpcmSampleAddress(info.endAddress))
        reader.Fail(std::format("the end address {} of an ADPCM wave points at a frame header", info.endAddress));

    if (!info.loops)
        return;

    if (info.encoding == WaveEncoding::DspAdpcm && !IsAdpcmSampleAddress(info.loopStartAddress))
        reader.Fail(std::format("the loop address {} of an ADPCM wave points at a frame header", info.loopStartAddress));

    if (info.LoopStartSample() >= info.SampleCount())
        reader.Fail("the loop of a wave starts behind its end");
}

WaveEncoding ReadEncoding(Formats::ByteReader& reader)
{
    std::uint8_t number = reader.ReadByte();
    if (number > static_cast<std::uint8_t>(WaveEncoding::DspAdpcm))
        reader.Fail(std::format("the wave encoding {} is not known", number));

    return static_cast<WaveEncoding>(number);
}


// A channel is described by its own record, which the table of channels points to. The ADPCM parameters
// lie somewhere else in the wave info and are pointed to as well.
WaveChannel ReadChannel(Formats::ByteView sectionBytes, std::size_t waveInfoOffset, std::uint32_t channelOffset, WaveEncoding encoding,
    std::string_view dataDescription)
{
    Formats::ByteReader reader(sectionBytes, dataDescription);
    reader.MoveTo(waveInfoOffset + channelOffset);

    WaveChannel channel{};
    channel.sampleDataOffset = reader.ReadUint32();
    std::uint32_t adpcmOffset = reader.ReadUint32();
    reader.Skip(VolumeFieldsSize + UnusedAtEndOfChannelSize);

    if (encoding != WaveEncoding::DspAdpcm)
        return channel;

    reader.MoveTo(waveInfoOffset + adpcmOffset);
    channel.adpcm = ReadAdpcmParameters(reader);
    return channel;
}

}

AdpcmParameters ReadAdpcmParameters(Formats::ByteReader& reader)
{
    AdpcmParameters parameters;
    for (std::int16_t& coefficient : parameters.coefficients)
        coefficient = reader.ReadInt16();

    std::uint16_t gain = reader.ReadUint16();
    if (gain != 0)
        reader.Fail(std::format("the ADPCM gain {} is not zero", gain));

    parameters.initialPredictorAndScale = reader.ReadUint16();
    parameters.initialHistory1 = reader.ReadInt16();
    parameters.initialHistory2 = reader.ReadInt16();
    parameters.loopPredictorAndScale = reader.ReadUint16();
    parameters.loopHistory1 = reader.ReadInt16();
    parameters.loopHistory2 = reader.ReadInt16();
    return parameters;
}

std::uint32_t WaveInfo::SampleCount() const
{
    if (encoding == WaveEncoding::DspAdpcm)
        return AdpcmSampleOfNibbleAddress(endAddress) + 1;

    return endAddress + 1;
}

std::uint32_t WaveInfo::LoopStartSample() const
{
    if (encoding == WaveEncoding::DspAdpcm)
        return AdpcmSampleOfNibbleAddress(loopStartAddress);

    return loopStartAddress;
}

std::uint32_t WaveInfo::ChannelByteCount() const
{
    std::uint32_t sampleCount = SampleCount();
    if (encoding == WaveEncoding::DspAdpcm)
        return AdpcmByteCount(sampleCount);

    if (encoding == WaveEncoding::Pcm16)
        return sampleCount * sizeof(std::int16_t);

    return sampleCount;
}

WaveInfo ReadWaveInfo(Formats::ByteView sectionBytes, std::size_t waveInfoOffset, std::string_view dataDescription)
{
    Formats::ByteReader reader(sectionBytes, dataDescription);
    reader.MoveTo(waveInfoOffset);

    WaveInfo info{};
    info.encoding = ReadEncoding(reader);
    info.loops = reader.ReadByte() != 0;
    std::uint8_t channelCount = reader.ReadByte();
    std::uint32_t sampleRateHighByte = reader.ReadByte();
    std::uint32_t sampleRateLowBytes = reader.ReadUint16();
    info.sampleRate = (sampleRateHighByte << SampleRateHighByteBits) | sampleRateLowBytes;

    std::uint8_t locationType = reader.ReadByte();
    if (locationType != OffsetLocationType)
        reader.Fail(std::format("the wave data location type {} is not supported, only 0 (an offset) is", locationType));

    reader.Skip(UnusedAfterLocationTypeSize);
    info.loopStartAddress = reader.ReadUint32();
    info.endAddress = reader.ReadUint32();
    std::uint32_t channelTableOffset = reader.ReadUint32();
    info.dataLocation = reader.ReadUint32();

    if (channelCount == 0 || channelCount > MaximumChannelCount)
        reader.Fail(std::format("a wave with {} channels is not supported, only mono and stereo are", channelCount));

    if (info.sampleRate == 0)
        reader.Fail("a wave has no sample rate");

    CheckAddresses(reader, info);

    reader.MoveTo(waveInfoOffset + channelTableOffset);
    std::vector<std::uint32_t> channelOffsets;
    for (std::uint8_t channelNumber = 0; channelNumber < channelCount; ++channelNumber)
        channelOffsets.push_back(reader.ReadUint32());

    for (std::uint32_t channelOffset : channelOffsets)
        info.channels.push_back(ReadChannel(sectionBytes, waveInfoOffset, channelOffset, info.encoding, dataDescription));

    return info;
}

}
