#include "Sound/WaveTable.h"

#include "Sound/AdpcmFrame.h"
#include "Sound/WaveDecoder.h"

#include "Formats/ByteReader.h"
#include "Formats/FormatError.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>

namespace Sound
{

namespace
{

constexpr std::size_t RegionSize = 0x18;
constexpr std::size_t ArticulationSize = 0x50;
constexpr std::size_t SampleSize = 0x10;
constexpr std::size_t AdpcmParameterSize = 0x2E;
constexpr std::uint32_t NibblesPerByte = 2;

// The table keeps times in timecents and loudness in tenths of a decibel, both times 65536. The smallest number means "none" (at once).
constexpr double FixedPointOne = 65536.0;
constexpr std::int32_t NoneValue = std::numeric_limits<std::int32_t>::min();
constexpr double TimecentsPerOctave = 1200.0;

enum class SampleFormat : std::uint16_t
{
    Adpcm = 0,
    Pcm8 = 1,
    Pcm16 = 2,
};

// The articulation of DLS, one 32 bit number each, in the order of the file.
struct Articulation
{
    std::int32_t lfoFrequency;
    std::int32_t lfoDelay;
    std::int32_t lfoAttenuation;
    std::int32_t lfoPitch;
    std::int32_t lfoModulationAttenuation;
    std::int32_t lfoModulationPitch;
    std::int32_t attack;
    std::int32_t decay;
    std::int32_t sustain;
    std::int32_t release;
    std::int32_t velocityToAttack;
    std::int32_t keyToDecay;
    std::int32_t secondAttack;
    std::int32_t secondDecay;
    std::int32_t secondSustain;
    std::int32_t secondRelease;
    std::int32_t secondVelocityToAttack;
    std::int32_t secondKeyToDecay;
    std::int32_t secondEnvelopeToPitch;
    std::int32_t pan;
};

struct TableOffsets
{
    std::uint32_t percussiveInstruments;
    std::uint32_t melodicInstruments;
    std::uint32_t regions;
    std::uint32_t articulations;
    std::uint32_t samples;
    std::uint32_t adpcmParameters;
};

TableOffsets ReadOffsets(Formats::ByteReader& reader)
{
    TableOffsets offsets;
    offsets.percussiveInstruments = reader.ReadUint32();
    offsets.melodicInstruments = reader.ReadUint32();
    offsets.regions = reader.ReadUint32();
    offsets.articulations = reader.ReadUint32();
    offsets.samples = reader.ReadUint32();
    offsets.adpcmParameters = reader.ReadUint32();
    return offsets;
}

std::int32_t ReadInt32(Formats::ByteReader& reader)
{
    return static_cast<std::int32_t>(reader.ReadUint32());
}

Articulation ReadArticulation(Formats::ByteReader& reader)
{
    Articulation articulation;
    articulation.lfoFrequency = ReadInt32(reader);
    articulation.lfoDelay = ReadInt32(reader);
    articulation.lfoAttenuation = ReadInt32(reader);
    articulation.lfoPitch = ReadInt32(reader);
    articulation.lfoModulationAttenuation = ReadInt32(reader);
    articulation.lfoModulationPitch = ReadInt32(reader);
    articulation.attack = ReadInt32(reader);
    articulation.decay = ReadInt32(reader);
    articulation.sustain = ReadInt32(reader);
    articulation.release = ReadInt32(reader);
    articulation.velocityToAttack = ReadInt32(reader);
    articulation.keyToDecay = ReadInt32(reader);
    articulation.secondAttack = ReadInt32(reader);
    articulation.secondDecay = ReadInt32(reader);
    articulation.secondSustain = ReadInt32(reader);
    articulation.secondRelease = ReadInt32(reader);
    articulation.secondVelocityToAttack = ReadInt32(reader);
    articulation.secondKeyToDecay = ReadInt32(reader);
    articulation.secondEnvelopeToPitch = ReadInt32(reader);
    articulation.pan = ReadInt32(reader);
    return articulation;
}

std::optional<double> SecondsOfTimecents(std::int32_t value)
{
    if (value == NoneValue)
        return std::nullopt;

    return std::pow(2.0, value / FixedPointOne / TimecentsPerOctave);
}

WaveTableEnvelope MakeEnvelope(const Articulation& articulation)
{
    bool movesTheLoudness = articulation.lfoAttenuation != 0 || articulation.lfoModulationAttenuation != 0;
    bool movesThePitch = articulation.lfoPitch != 0 || articulation.lfoModulationPitch != 0 || articulation.secondEnvelopeToPitch != 0;
    bool scalesTheTimes = articulation.velocityToAttack != NoneValue || articulation.keyToDecay != NoneValue;
    if (movesTheLoudness || movesThePitch || scalesTheTimes)
        throw Formats::FormatError("The wave table uses an LFO, the second envelope or scaled times, which this synth does not play");

    WaveTableEnvelope envelope;
    envelope.attackSeconds = SecondsOfTimecents(articulation.attack);
    envelope.decaySeconds = SecondsOfTimecents(articulation.decay);
    envelope.sustainTenthsOfDecibel = articulation.sustain / FixedPointOne;
    envelope.releaseTenthsOfDecibelPerFrame = articulation.release / FixedPointOne;
    return envelope;
}

AdpcmParameters ReadAdpcmParameters(Formats::ByteReader& reader)
{
    AdpcmParameters parameters;
    constexpr std::size_t GainSize = 2;
    for (std::int16_t& coefficient : parameters.coefficients)
        coefficient = reader.ReadInt16();
    reader.Skip(GainSize);
    parameters.initialPredictorAndScale = reader.ReadUint16();
    parameters.initialHistory1 = reader.ReadInt16();
    parameters.initialHistory2 = reader.ReadInt16();
    parameters.loopPredictorAndScale = reader.ReadUint16();
    parameters.loopHistory1 = reader.ReadInt16();
    parameters.loopHistory2 = reader.ReadInt16();
    return parameters;
}

Formats::ByteView PartOfSampleFile(Formats::ByteView sampleFile, std::size_t byteOffset, std::size_t byteCount)
{
    if (byteOffset > sampleFile.size() || byteCount > sampleFile.size() - byteOffset)
        throw Formats::FormatError("A sample of the wave table lies beyond the end of its sample file");
    return sampleFile.subspan(byteOffset, byteCount);
}

// The sample file keeps of the last frame of a wave only the nibbles it needs: its header and its samples.
std::size_t StoredAdpcmByteCount(std::uint32_t sampleCount)
{
    std::uint32_t wholeFrames = sampleCount / AdpcmSamplesPerFrame;
    std::uint32_t samplesInTheLastFrame = sampleCount % AdpcmSamplesPerFrame;
    std::uint32_t nibbles = wholeFrames * AdpcmNibblesPerFrame;
    if (samplesInTheLastFrame > 0)
        nibbles += AdpcmHeaderNibbleCount + samplesInTheLastFrame;
    return (nibbles + 1) / NibblesPerByte;
}

// A sample of the table: its format, its rate, where it starts in the sample file and how many samples it has.
struct SampleEntry
{
    SampleFormat format;
    std::uint32_t sampleRate;
    std::uint32_t offset;
    std::uint32_t sampleCount;
    std::uint16_t adpcmIndex;
};

SampleEntry ReadSampleEntry(Formats::ByteReader& reader)
{
    SampleEntry entry;
    entry.format = static_cast<SampleFormat>(reader.ReadUint16());
    entry.sampleRate = reader.ReadUint16();
    entry.offset = reader.ReadUint32();
    entry.sampleCount = reader.ReadUint32();
    entry.adpcmIndex = reader.ReadUint16();
    return entry;
}

// The offset of a sample counts in the units of its format: nibbles for ADPCM, bytes for 8 bit and 16 bit words for 16 bit samples.
std::vector<float> DecodeSamples(const SampleEntry& entry, Formats::ByteView tableFile, std::size_t adpcmTableOffset, Formats::ByteView sampleFile)
{
    constexpr std::uint32_t BytesPerPcm16Sample = 2;
    switch (entry.format)
    {
    case SampleFormat::Adpcm:
    {
        Formats::ByteReader adpcmReader(tableFile, "wave table ADPCM parameters");
        adpcmReader.MoveTo(adpcmTableOffset + entry.adpcmIndex * AdpcmParameterSize);
        AdpcmParameters parameters = ReadAdpcmParameters(adpcmReader);
        Formats::ByteView stored = PartOfSampleFile(sampleFile, entry.offset / NibblesPerByte, StoredAdpcmByteCount(entry.sampleCount));
        Formats::Bytes frames(stored.begin(), stored.end());
        frames.resize(AdpcmByteCount(entry.sampleCount), 0);
        return DecodeAdpcm(frames, parameters, entry.sampleCount);
    }
    case SampleFormat::Pcm8:
        return DecodePcm8(PartOfSampleFile(sampleFile, entry.offset, entry.sampleCount), entry.sampleCount);
    case SampleFormat::Pcm16:
    {
        std::size_t byteOffset = std::size_t(entry.offset) * BytesPerPcm16Sample;
        std::size_t byteCount = std::size_t(entry.sampleCount) * BytesPerPcm16Sample;
        return DecodePcm16(PartOfSampleFile(sampleFile, byteOffset, byteCount), entry.sampleCount);
    }
    }
    throw Formats::FormatError(std::format("A sample of the wave table has the unknown format {}", static_cast<int>(entry.format)));
}

std::vector<std::vector<std::uint16_t>> ReadInstruments(Formats::ByteReader& reader, const TableOffsets& offsets)
{
    constexpr std::size_t InstrumentSize = WaveTable::KeyCount * sizeof(std::uint16_t);
    std::size_t instrumentCount = (offsets.regions - offsets.melodicInstruments) / InstrumentSize;
    reader.MoveTo(offsets.melodicInstruments);

    std::vector<std::vector<std::uint16_t>> instruments;
    for (std::size_t instrument = 0; instrument < instrumentCount; ++instrument)
    {
        std::vector<std::uint16_t> regionOfKey(WaveTable::KeyCount);
        for (std::uint16_t& region : regionOfKey)
            region = reader.ReadUint16();
        instruments.push_back(std::move(regionOfKey));
    }
    return instruments;
}

using Waves = std::vector<std::shared_ptr<const WaveTableWave>>;

Waves ReadWaves(Formats::ByteReader& reader, const TableOffsets& offsets, Formats::ByteView tableFile, Formats::ByteView sampleFile)
{
    std::size_t sampleCount = (offsets.adpcmParameters - offsets.samples) / SampleSize;
    Waves waves;
    for (std::size_t sample = 0; sample < sampleCount; ++sample)
    {
        reader.MoveTo(offsets.samples + sample * SampleSize);
        SampleEntry entry = ReadSampleEntry(reader);
        auto wave = std::make_shared<WaveTableWave>();
        wave->sampleRate = entry.sampleRate;
        wave->samples = DecodeSamples(entry, tableFile, offsets.adpcmParameters, sampleFile);
        waves.push_back(std::move(wave));
    }
    return waves;
}

// A copy of the wave that ends with its loop and plays the loop again and again.
std::shared_ptr<const WaveTableWave> MakeLoopingWave(const WaveTableWave& wave, std::uint32_t loopStart, std::uint32_t loopLength)
{
    auto loopingWave = std::make_shared<WaveTableWave>(wave);
    std::size_t loopEnd = std::size_t(loopStart) + loopLength;
    loopingWave->samples.resize(std::min(loopingWave->samples.size(), loopEnd));
    loopingWave->loops = true;
    loopingWave->loopStartSample = loopStart;
    return loopingWave;
}

WaveTableRegion ReadRegion(Formats::ByteReader& reader, const TableOffsets& offsets, Formats::ByteView tableFile, const Waves& waves)
{
    constexpr std::size_t UnusedByteSize = 1;
    WaveTableRegion region;
    region.unityKey = reader.ReadByte();
    reader.Skip(UnusedByteSize);
    region.fineTuneCents = reader.ReadInt16();
    region.attenuationTenthsOfDecibel = ReadInt32(reader) / FixedPointOne;
    std::uint32_t loopStart = reader.ReadUint32();
    std::uint32_t loopLength = reader.ReadUint32();
    std::uint32_t articulationIndex = reader.ReadUint32();
    std::uint32_t sampleIndex = reader.ReadUint32();
    if (sampleIndex >= waves.size())
        throw Formats::FormatError(std::format("A region of the wave table names the sample {}, which it does not have", sampleIndex));

    Formats::ByteReader articulationReader(tableFile, "wave table articulation");
    articulationReader.MoveTo(offsets.articulations + articulationIndex * ArticulationSize);
    region.envelope = MakeEnvelope(ReadArticulation(articulationReader));

    if (loopLength == 0)
        region.wave = waves[sampleIndex];
    else
        region.wave = MakeLoopingWave(*waves[sampleIndex], loopStart, loopLength);
    return region;
}

}

WaveTable::WaveTable(Formats::ByteView tableFile, Formats::ByteView sampleFile)
{
    Formats::ByteReader reader(tableFile, "wave table");
    TableOffsets offsets = ReadOffsets(reader);
    instruments = ReadInstruments(reader, offsets);
    Waves waves = ReadWaves(reader, offsets, tableFile, sampleFile);

    std::size_t regionCount = (offsets.articulations - offsets.regions) / RegionSize;
    for (std::size_t regionNumber = 0; regionNumber < regionCount; ++regionNumber)
    {
        reader.MoveTo(offsets.regions + regionNumber * RegionSize);
        regions.push_back(ReadRegion(reader, offsets, tableFile, waves));
    }
}

const WaveTableRegion* WaveTable::FindRegion(std::uint8_t program, std::uint8_t key) const
{
    if (program >= instruments.size() || key >= KeyCount)
        return nullptr;

    std::uint16_t region = instruments[program][key];
    if (region == NoRegion || region >= regions.size())
        return nullptr;

    return &regions[region];
}

std::size_t WaveTable::InstrumentCount() const
{
    return instruments.size();
}

}
