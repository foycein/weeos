#include "Media/QuickTimeReader.h"

#include "Formats/ByteReader.h"
#include "Formats/FormatError.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>

namespace Media
{

namespace
{

constexpr std::string_view DataDescription = "QuickTime movie";

// A box (atom) is its size, its four character type and its body. A size of 1 means a 64 bit size follows the type,
// a size of 0 means the box reaches to the end of the file.
constexpr std::size_t FourCharacters = 4;
constexpr std::size_t BoxHeaderSize = 2 * FourCharacters;
constexpr std::uint32_t SizeIsSixtyFourBits = 1;
constexpr std::uint32_t SizeReachesTheEnd = 0;

constexpr std::string_view MovieBox = "moov";
constexpr std::string_view TrackBox = "trak";
constexpr std::string_view MediaBox = "mdia";
constexpr std::string_view MediaHeaderBox = "mdhd";
constexpr std::string_view HandlerBox = "hdlr";
constexpr std::string_view MediaInformationBox = "minf";
constexpr std::string_view SampleTableBox = "stbl";
constexpr std::string_view SampleDescriptionBox = "stsd";
constexpr std::string_view TimeToSampleBox = "stts";
constexpr std::string_view SampleToChunkBox = "stsc";
constexpr std::string_view SampleSizeBox = "stsz";
constexpr std::string_view ChunkOffsetBox = "stco";
constexpr std::string_view LargeChunkOffsetBox = "co64";
constexpr std::string_view FieldsBox = "fiel";
constexpr std::string_view ElementaryStreamBox = "esds";

constexpr std::string_view VideoHandler = "vide";
constexpr std::string_view SoundHandler = "soun";

constexpr std::string_view JpegCodec = "jpeg";
constexpr std::string_view MotionJpegACodec = "mjpa";
constexpr std::string_view MotionJpegBCodec = "mjpb";
constexpr std::string_view UnsignedEightBitCodec = "raw ";
constexpr std::string_view BigEndianCodec = "twos";
constexpr std::string_view LittleEndianCodec = "sowt";
constexpr std::string_view MuLawCodec = "ulaw";
constexpr std::string_view ALawCodec = "alaw";
constexpr std::string_view AacCodec = "mp4a";

// The places inside a sample description entry, counted from the start of the entry (its size and its codec come first).
constexpr std::size_t EntryCodecPlace = 4;
constexpr std::size_t EntryVersionPlace = 16;
constexpr std::size_t VideoWidthPlace = 32;
constexpr std::size_t VideoBoxesPlace = 86;
constexpr std::size_t SoundChannelsPlace = 24;
constexpr std::size_t SoundRatePlace = 32;
constexpr std::size_t SoundBoxesPlaceOfVersion0 = 36;
constexpr std::size_t SoundBoxesPlaceOfVersion1 = 52;
constexpr std::size_t SoundBoxesPlaceOfVersion2 = 72;
constexpr int SoundVersion1 = 1;
constexpr int SoundVersion2 = 2;
constexpr int FixedPointFractionBits = 16;

// The descriptors of an "esds" box (MPEG-4 Systems): the elementary stream, its decoder configuration and the setup of the decoder.
constexpr std::size_t VersionAndFlagsSize = 4;
constexpr std::size_t FlagsSize = 3;
constexpr Formats::Byte MediaHeaderWithLongTimes = 1;
constexpr std::size_t CreationAndChangeTimes = 2;
constexpr std::uint8_t ElementaryStreamDescriptor = 0x03;
constexpr std::uint8_t DecoderConfigDescriptor = 0x04;
constexpr std::uint8_t DecoderSetupDescriptor = 0x05;
constexpr std::uint8_t DependsOnStreamFlag = 0x80;
constexpr std::uint8_t UrlFlag = 0x40;
constexpr std::uint8_t ClockReferenceFlag = 0x20;
constexpr std::size_t DecoderConfigFixedSize = 13;
constexpr int DescriptorLengthBytes = 4;
constexpr std::uint8_t LengthContinues = 0x80;
constexpr std::uint8_t LengthBits = 0x7F;
constexpr int LengthBitsPerByte = 7;

constexpr int EightBits = 8;
constexpr int SixteenBits = 16;

struct Box
{
    std::string type;
    Formats::ByteView body;
};

std::string ReadFourCharacters(Formats::ByteReader& reader)
{
    Formats::ByteView characters = reader.ReadBytes(FourCharacters);
    return std::string(characters.begin(), characters.end());
}

std::vector<Box> ReadBoxes(Formats::ByteView data)
{
    std::vector<Box> boxes;
    Formats::ByteReader reader(data, DataDescription);
    while (reader.RemainingSize() >= BoxHeaderSize)
    {
        std::size_t boxStart = reader.Position();
        std::uint64_t size = reader.ReadUint32();
        std::string type = ReadFourCharacters(reader);
        if (size == SizeIsSixtyFourBits)
            size = reader.ReadUint64();
        else if (size == SizeReachesTheEnd)
            size = data.size() - boxStart;

        std::size_t headerSize = reader.Position() - boxStart;
        bool isWholeBox = size >= headerSize && Formats::IsRangeInside(boxStart, size, data.size());
        if (!isWholeBox)
            break;

        boxes.push_back({type, reader.ReadBytes(static_cast<std::size_t>(size - headerSize))});
    }
    return boxes;
}

std::optional<Formats::ByteView> FindBox(Formats::ByteView data, std::string_view type)
{
    for (const Box& box : ReadBoxes(data))
    {
        if (box.type == type)
            return box.body;
    }
    return std::nullopt;
}

Formats::ByteView FindBoxOrFail(Formats::ByteView data, std::string_view type)
{
    std::optional<Formats::ByteView> body = FindBox(data, type);
    if (!body)
        throw Formats::FormatError(std::string(DataDescription) + ": a track has no box " + std::string(type));

    return *body;
}

// Looks for a box among the boxes and inside every box below them (the "esds" of a QuickTime file is inside a "wave" box).
std::optional<Formats::ByteView> FindBoxAnywhere(Formats::ByteView data, std::string_view type)
{
    for (const Box& box : ReadBoxes(data))
    {
        if (box.type == type)
            return box.body;

        std::optional<Formats::ByteView> inside = FindBoxAnywhere(box.body, type);
        if (inside)
            return inside;
    }
    return std::nullopt;
}

// The sample tables of a track: the size of every sample, the start of every chunk, and how many samples each chunk holds.
struct SampleTables
{
    std::vector<std::uint32_t> sampleSizes;
    std::vector<std::uint64_t> chunkPlaces;
    std::vector<std::uint32_t> samplesInChunk;
    std::vector<std::uint32_t> sampleDurations;
};

std::vector<std::uint32_t> ReadSampleSizes(Formats::ByteView body)
{
    Formats::ByteReader reader(body, DataDescription);
    reader.Skip(VersionAndFlagsSize);
    std::uint32_t sizeOfEverySample = reader.ReadUint32();
    std::uint32_t sampleCount = reader.ReadUint32();

    if (sizeOfEverySample != 0)
        return std::vector<std::uint32_t>(sampleCount, sizeOfEverySample);

    std::vector<std::uint32_t> sizes;
    for (std::uint32_t sampleNumber = 0; sampleNumber < sampleCount; ++sampleNumber)
        sizes.push_back(reader.ReadUint32());

    return sizes;
}

std::vector<std::uint64_t> ReadChunkPlaces(Formats::ByteView sampleTableBody)
{
    std::optional<Formats::ByteView> largeOffsets = FindBox(sampleTableBody, LargeChunkOffsetBox);
    bool isLarge = largeOffsets.has_value();
    Formats::ByteView offsets = isLarge ? *largeOffsets : FindBoxOrFail(sampleTableBody, ChunkOffsetBox);
    Formats::ByteReader reader(offsets, DataDescription);
    reader.Skip(VersionAndFlagsSize);
    std::uint32_t chunkCount = reader.ReadUint32();

    std::vector<std::uint64_t> places;
    for (std::uint32_t chunkNumber = 0; chunkNumber < chunkCount; ++chunkNumber)
        places.push_back(isLarge ? reader.ReadUint64() : reader.ReadUint32());

    return places;
}

// From its first chunk on (counted from 1) every chunk of a run holds the same number of samples.
struct ChunkRun
{
    std::uint32_t firstChunk = 0;
    std::uint32_t samplesPerChunk = 0;
};

// The table names runs of chunks; a run lasts up to the first chunk of the next run.
std::vector<std::uint32_t> ReadSamplesInChunks(Formats::ByteView body, std::size_t chunkCount)
{
    Formats::ByteReader reader(body, DataDescription);
    reader.Skip(VersionAndFlagsSize);
    std::uint32_t runCount = reader.ReadUint32();

    std::vector<ChunkRun> runs;
    for (std::uint32_t runNumber = 0; runNumber < runCount; ++runNumber)
    {
        ChunkRun run;
        run.firstChunk = reader.ReadUint32();
        run.samplesPerChunk = reader.ReadUint32();
        reader.Skip(sizeof(std::uint32_t));
        runs.push_back(run);
    }

    std::vector<std::uint32_t> samplesInChunk(chunkCount, 0);
    for (std::size_t runIndex = 0; runIndex < runs.size(); ++runIndex)
    {
        std::size_t firstChunkIndex = runs[runIndex].firstChunk - 1;
        std::size_t endChunkIndex = chunkCount;
        bool hasNextRun = runIndex + 1 < runs.size();
        if (hasNextRun)
            endChunkIndex = std::min<std::size_t>(runs[runIndex + 1].firstChunk - 1, chunkCount);

        for (std::size_t chunkIndex = firstChunkIndex; chunkIndex < endChunkIndex; ++chunkIndex)
            samplesInChunk[chunkIndex] = runs[runIndex].samplesPerChunk;
    }
    return samplesInChunk;
}

std::vector<std::uint32_t> ReadSampleDurations(Formats::ByteView body)
{
    Formats::ByteReader reader(body, DataDescription);
    reader.Skip(VersionAndFlagsSize);
    std::uint32_t runCount = reader.ReadUint32();

    std::vector<std::uint32_t> durations;
    for (std::uint32_t runNumber = 0; runNumber < runCount; ++runNumber)
    {
        std::uint32_t sampleCount = reader.ReadUint32();
        std::uint32_t duration = reader.ReadUint32();
        durations.insert(durations.end(), sampleCount, duration);
    }
    return durations;
}

SampleTables ReadSampleTables(Formats::ByteView sampleTableBody)
{
    SampleTables tables;
    tables.sampleSizes = ReadSampleSizes(FindBoxOrFail(sampleTableBody, SampleSizeBox));
    tables.chunkPlaces = ReadChunkPlaces(sampleTableBody);
    tables.samplesInChunk = ReadSamplesInChunks(FindBoxOrFail(sampleTableBody, SampleToChunkBox), tables.chunkPlaces.size());
    tables.sampleDurations = ReadSampleDurations(FindBoxOrFail(sampleTableBody, TimeToSampleBox));
    return tables;
}

// Every sample on its own: the samples of a chunk lie one after the other from the start of the chunk.
std::vector<MoviePiece> EverySample(const SampleTables& tables)
{
    std::vector<MoviePiece> pieces;
    std::size_t sampleIndex = 0;
    for (std::size_t chunkIndex = 0; chunkIndex < tables.chunkPlaces.size(); ++chunkIndex)
    {
        std::uint64_t place = tables.chunkPlaces[chunkIndex];
        for (std::uint32_t sampleInChunk = 0; sampleInChunk < tables.samplesInChunk[chunkIndex]; ++sampleInChunk)
        {
            if (sampleIndex >= tables.sampleSizes.size())
                return pieces;

            std::uint32_t size = tables.sampleSizes[sampleIndex];
            pieces.push_back({place, size});
            place += size;
            ++sampleIndex;
        }
    }
    return pieces;
}

// Uncompressed sound counts its samples one per sound frame (one value for each channel), so a chunk is a run of whole frames.
std::vector<MoviePiece> EveryChunkOfFrames(const SampleTables& tables, int bytesPerFrame)
{
    std::vector<MoviePiece> pieces;
    for (std::size_t chunkIndex = 0; chunkIndex < tables.chunkPlaces.size(); ++chunkIndex)
    {
        std::uint32_t size = tables.samplesInChunk[chunkIndex] * static_cast<std::uint32_t>(bytesPerFrame);
        pieces.push_back({tables.chunkPlaces[chunkIndex], size});
    }
    return pieces;
}

struct MediaHeader
{
    std::uint32_t timeScale = 0;
    std::uint64_t duration = 0;
};

MediaHeader ReadMediaHeader(Formats::ByteView body)
{
    Formats::ByteReader reader(body, DataDescription);
    bool hasLongTimes = reader.ReadByte() == MediaHeaderWithLongTimes;
    reader.Skip(FlagsSize);

    // The time of creation and of the last change come first; the media header of version 1 holds them in 64 bits.
    MediaHeader header;
    if (hasLongTimes)
    {
        reader.Skip(CreationAndChangeTimes * sizeof(std::uint64_t));
        header.timeScale = reader.ReadUint32();
        header.duration = reader.ReadUint64();
    }
    else
    {
        reader.Skip(CreationAndChangeTimes * sizeof(std::uint32_t));
        header.timeScale = reader.ReadUint32();
        header.duration = reader.ReadUint32();
    }

    if (header.timeScale == 0)
        reader.Fail("a track has a time scale of 0");

    return header;
}

std::string ReadHandlerType(Formats::ByteView body)
{
    Formats::ByteReader reader(body, DataDescription);
    reader.Skip(VersionAndFlagsSize + FourCharacters);
    return ReadFourCharacters(reader);
}

// The first entry of the sample description box.
Formats::ByteView FirstSampleDescription(Formats::ByteView sampleTableBody)
{
    Formats::ByteReader reader(FindBoxOrFail(sampleTableBody, SampleDescriptionBox), DataDescription);
    reader.Skip(VersionAndFlagsSize);
    if (reader.ReadUint32() == 0)
        reader.Fail("a track has no sample description");

    std::size_t entryStart = reader.Position();
    std::uint32_t entrySize = reader.ReadUint32();
    reader.MoveTo(entryStart);
    return reader.ReadBytes(entrySize);
}

std::string CodecOf(Formats::ByteView entry)
{
    Formats::ByteReader reader(entry, DataDescription);
    reader.MoveTo(EntryCodecPlace);
    return ReadFourCharacters(reader);
}

MovieVideoKind VideoKindOf(const std::string& codec)
{
    if (codec == JpegCodec || codec == MotionJpegACodec)
        return MovieVideoKind::MotionJpeg;

    if (codec == MotionJpegBCodec)
        return MovieVideoKind::MotionJpegB;

    return MovieVideoKind::Unsupported;
}

MovieSoundKind SoundKindOf(const std::string& codec, int bitsPerSample)
{
    if (codec == UnsignedEightBitCodec)
        return MovieSoundKind::Unsigned8Bit;

    bool isTwosComplement = codec == BigEndianCodec || codec == LittleEndianCodec;
    if (isTwosComplement && bitsPerSample == EightBits)
        return MovieSoundKind::Signed8Bit;

    if (codec == BigEndianCodec && bitsPerSample == SixteenBits)
        return MovieSoundKind::BigEndian16Bit;

    if (codec == LittleEndianCodec && bitsPerSample == SixteenBits)
        return MovieSoundKind::LittleEndian16Bit;

    if (codec == MuLawCodec)
        return MovieSoundKind::MuLaw;

    if (codec == ALawCodec)
        return MovieSoundKind::ALaw;

    if (codec == AacCodec)
        return MovieSoundKind::Aac;

    return MovieSoundKind::Unsupported;
}

int BytesPerSoundFrame(const MovieSoundTrack& sound)
{
    bool isOneBytePerValue = sound.kind == MovieSoundKind::MuLaw || sound.kind == MovieSoundKind::ALaw
        || sound.kind == MovieSoundKind::Unsigned8Bit || sound.kind == MovieSoundKind::Signed8Bit;
    int bytesPerValue = isOneBytePerValue ? 1 : SixteenBits / EightBits;
    return sound.channelCount * bytesPerValue;
}

std::size_t ReadDescriptorLength(Formats::ByteReader& reader)
{
    std::size_t length = 0;
    for (int byteNumber = 0; byteNumber < DescriptorLengthBytes; ++byteNumber)
    {
        Formats::Byte part = reader.ReadByte();
        length = (length << LengthBitsPerByte) | (part & LengthBits);
        if ((part & LengthContinues) == 0)
            break;
    }
    return length;
}

// The AudioSpecificConfig inside the "esds" box: the elementary stream descriptor holds the decoder configuration,
// which holds the decoder's setup.
Formats::Bytes ReadDecoderSetup(Formats::ByteView elementaryStreamBody)
{
    Formats::ByteReader reader(elementaryStreamBody, DataDescription);
    reader.Skip(VersionAndFlagsSize);
    if (reader.ReadByte() != ElementaryStreamDescriptor)
        reader.Fail("the esds box does not start with an elementary stream descriptor");

    ReadDescriptorLength(reader);
    reader.Skip(sizeof(std::uint16_t));
    Formats::Byte flags = reader.ReadByte();
    if ((flags & DependsOnStreamFlag) != 0)
        reader.Skip(sizeof(std::uint16_t));
    if ((flags & UrlFlag) != 0)
        reader.Skip(reader.ReadByte());
    if ((flags & ClockReferenceFlag) != 0)
        reader.Skip(sizeof(std::uint16_t));

    if (reader.ReadByte() != DecoderConfigDescriptor)
        reader.Fail("the elementary stream has no decoder configuration");

    ReadDescriptorLength(reader);
    reader.Skip(DecoderConfigFixedSize);
    if (reader.ReadByte() != DecoderSetupDescriptor)
        reader.Fail("the decoder configuration has no decoder setup");

    std::size_t setupLength = ReadDescriptorLength(reader);
    Formats::ByteView setup = reader.ReadBytes(setupLength);
    return Formats::Bytes(setup.begin(), setup.end());
}

MovieVideoTrack MakeVideoTrack(Formats::ByteView sampleTableBody, const MediaHeader& header)
{
    Formats::ByteView entry = FirstSampleDescription(sampleTableBody);
    Formats::ByteReader reader(entry, DataDescription);

    MovieVideoTrack video;
    video.codecName = CodecOf(entry);
    video.kind = VideoKindOf(video.codecName);
    reader.MoveTo(VideoWidthPlace);
    video.width = reader.ReadUint16();
    video.height = reader.ReadUint16();

    if (entry.size() > VideoBoxesPlace)
    {
        std::optional<Formats::ByteView> fields = FindBox(entry.subspan(VideoBoxesPlace), FieldsBox);
        if (fields && !fields->empty())
            video.fieldCount = (*fields)[0];
    }

    SampleTables tables = ReadSampleTables(sampleTableBody);
    video.frames = EverySample(tables);

    std::uint64_t time = 0;
    for (std::size_t frameIndex = 0; frameIndex < video.frames.size(); ++frameIndex)
    {
        video.frameStartSeconds.push_back(static_cast<double>(time) / header.timeScale);
        if (frameIndex < tables.sampleDurations.size())
            time += tables.sampleDurations[frameIndex];
    }
    return video;
}

// The boxes of a sound description follow its fields, which are longer in the versions 1 and 2.
std::size_t SoundBoxesPlace(int version)
{
    if (version == SoundVersion1)
        return SoundBoxesPlaceOfVersion1;

    if (version == SoundVersion2)
        return SoundBoxesPlaceOfVersion2;

    return SoundBoxesPlaceOfVersion0;
}

std::optional<Formats::ByteView> FindElementaryStreamBox(Formats::ByteView entry, int version)
{
    std::size_t boxesPlace = SoundBoxesPlace(version);
    if (entry.size() <= boxesPlace)
        return std::nullopt;

    return FindBoxAnywhere(entry.subspan(boxesPlace), ElementaryStreamBox);
}

MovieSoundTrack MakeSoundTrack(Formats::ByteView sampleTableBody)
{
    Formats::ByteView entry = FirstSampleDescription(sampleTableBody);
    Formats::ByteReader reader(entry, DataDescription);

    MovieSoundTrack sound;
    sound.codecName = CodecOf(entry);
    reader.MoveTo(EntryVersionPlace);
    int version = reader.ReadUint16();
    reader.MoveTo(SoundChannelsPlace);
    sound.channelCount = reader.ReadUint16();
    sound.bitsPerSample = reader.ReadUint16();
    reader.MoveTo(SoundRatePlace);
    sound.sampleRate = static_cast<int>(reader.ReadUint32() >> FixedPointFractionBits);
    sound.kind = SoundKindOf(sound.codecName, sound.bitsPerSample);

    SampleTables tables = ReadSampleTables(sampleTableBody);
    if (sound.kind == MovieSoundKind::Aac)
    {
        std::optional<Formats::ByteView> elementaryStreamBody = FindElementaryStreamBox(entry, version);
        if (!elementaryStreamBody)
            reader.Fail("an AAC track has no esds box");

        sound.decoderSetup = ReadDecoderSetup(*elementaryStreamBody);
        sound.pieces = EverySample(tables);
    }
    else if (sound.kind != MovieSoundKind::Unsupported)
    {
        sound.pieces = EveryChunkOfFrames(tables, BytesPerSoundFrame(sound));
    }
    return sound;
}

}

Movie ReadQuickTimeMovie(Formats::ByteView fileBytes)
{
    std::optional<Formats::ByteView> movieBox = FindBox(fileBytes, MovieBox);
    if (!movieBox)
        throw Formats::FormatError(std::string(DataDescription) + ": the file has no movie box");

    Movie movie;
    for (const Box& box : ReadBoxes(*movieBox))
    {
        if (box.type != TrackBox)
            continue;

        Formats::ByteView media = FindBoxOrFail(box.body, MediaBox);
        MediaHeader header = ReadMediaHeader(FindBoxOrFail(media, MediaHeaderBox));
        std::string handler = ReadHandlerType(FindBoxOrFail(media, HandlerBox));
        Formats::ByteView mediaInformation = FindBoxOrFail(media, MediaInformationBox);
        Formats::ByteView sampleTableBody = FindBoxOrFail(mediaInformation, SampleTableBox);

        double durationSeconds = static_cast<double>(header.duration) / header.timeScale;
        if (handler == VideoHandler && !movie.video)
        {
            movie.video = MakeVideoTrack(sampleTableBody, header);
            movie.durationSeconds = std::max(movie.durationSeconds, durationSeconds);
        }
        else if (handler == SoundHandler && !movie.sound)
        {
            movie.sound = MakeSoundTrack(sampleTableBody);
            movie.durationSeconds = std::max(movie.durationSeconds, durationSeconds);
        }
    }
    return movie;
}

}
