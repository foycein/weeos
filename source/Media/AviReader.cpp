#include "Media/AviReader.h"

#include "Formats/ByteReader.h"
#include "Formats/FormatError.h"

#include <algorithm>
#include <cstdlib>
#include <format>
#include <string>
#include <string_view>

namespace Media
{

namespace
{

constexpr std::string_view DataDescription = "AVI movie";

constexpr std::size_t FourCharacters = 4;
constexpr std::size_t ChunkHeaderSize = 8;
constexpr std::string_view RiffChunk = "RIFF";
constexpr std::string_view ListChunk = "LIST";
constexpr std::string_view AviForm = "AVI ";
constexpr std::string_view AviExtensionForm = "AVIX";
constexpr std::string_view StreamHeaderChunk = "strh";
constexpr std::string_view StreamFormatChunk = "strf";
constexpr std::string_view VideoStream = "vids";
constexpr std::string_view SoundStream = "auds";
constexpr std::string_view CompressedFrame = "dc";
constexpr std::string_view UncompressedFrame = "db";
constexpr std::string_view SoundData = "wb";
constexpr std::string_view MotionJpegCodec = "MJPG";
constexpr std::size_t StreamHeaderTimeScalePlace = 20;
constexpr std::size_t BitmapHeaderCodecPlace = 16;
constexpr int DecimalBase = 10;
constexpr std::size_t StreamNumberDigits = 2;
constexpr int NoStreamNumber = -1;

// The WAVE format tags of the sound that the Photo Channel plays.
constexpr std::uint16_t PcmFormat = 0x0001;
constexpr std::uint16_t MuLawFormat = 0x0007;
constexpr std::uint16_t ImaAdpcmFormat = 0x0011;
constexpr int EightBits = 8;
constexpr int SixteenBits = 16;

// An IMA ADPCM block starts with 4 bytes for each channel (the first sample and the step) and holds two samples per byte after that.
constexpr int ImaBlockHeaderSize = 4;
constexpr int ImaSamplesPerByte = 2;

struct AviStream
{
    std::string kind;
    std::uint32_t timeScale = 1;
    std::uint32_t rate = 0;
    Formats::ByteView format;
    std::vector<MoviePiece> pieces;
};

std::string ReadFourCharacters(Formats::ByteReader& reader)
{
    Formats::ByteView characters = reader.ReadBytes(FourCharacters);
    return std::string(characters.begin(), characters.end());
}

bool IsDecimalDigit(char character)
{
    return character >= '0' && character <= '9';
}

// Chunks of data are named by the number of their stream (two digits) and what they hold: "00dc" is a frame of stream 0.
int StreamNumberOfDataChunk(const std::string& chunkName)
{
    if (!IsDecimalDigit(chunkName[0]) || !IsDecimalDigit(chunkName[1]))
        return NoStreamNumber;

    std::string_view content = std::string_view(chunkName).substr(StreamNumberDigits);
    bool isData = content == CompressedFrame || content == UncompressedFrame || content == SoundData;
    if (!isData)
        return NoStreamNumber;

    return (chunkName[0] - '0') * DecimalBase + (chunkName[1] - '0');
}

class AviFileReader
{
public:
    explicit AviFileReader(Formats::ByteView bytes)
        : fileBytes(bytes)
    {
    }

    Movie Read()
    {
        Formats::ByteReader reader(fileBytes, DataDescription);
        bool hasAviForm = false;
        while (reader.RemainingSize() >= ChunkHeaderSize)
        {
            std::string chunkName = ReadFourCharacters(reader);
            std::uint32_t chunkSize = reader.ReadLittleEndianUint32();
            Formats::ByteView body = reader.ReadBytes(std::min<std::size_t>(chunkSize, reader.RemainingSize()));
            SkipPadding(reader, chunkSize);

            if (chunkName != RiffChunk || body.size() < FourCharacters)
                continue;

            std::string form(body.begin(), body.begin() + FourCharacters);
            if (form == AviForm)
                hasAviForm = true;

            if (form == AviForm || form == AviExtensionForm)
                ReadChunks(body.subspan(FourCharacters));
        }

        if (!hasAviForm)
            reader.Fail("the file is no AVI file");

        return MakeMovie();
    }

private:
    // Chunks are padded to an even size.
    static void SkipPadding(Formats::ByteReader& reader, std::uint32_t chunkSize)
    {
        if (chunkSize % 2 == 1 && !reader.IsAtEnd())
            reader.Skip(1);
    }

    void ReadChunks(Formats::ByteView chunks)
    {
        Formats::ByteReader reader(chunks, DataDescription);
        while (reader.RemainingSize() >= ChunkHeaderSize)
        {
            std::string chunkName = ReadFourCharacters(reader);
            std::uint32_t chunkSize = reader.ReadLittleEndianUint32();
            Formats::ByteView body = reader.ReadBytes(std::min<std::size_t>(chunkSize, reader.RemainingSize()));
            SkipPadding(reader, chunkSize);
            ReadChunk(chunkName, body);
        }
    }

    void ReadChunk(const std::string& chunkName, Formats::ByteView body)
    {
        if (chunkName == ListChunk && body.size() >= FourCharacters)
        {
            ReadChunks(body.subspan(FourCharacters));
            return;
        }

        if (chunkName == StreamHeaderChunk)
        {
            ReadStreamHeader(body);
            return;
        }

        if (chunkName == StreamFormatChunk && !streams.empty())
        {
            streams.back().format = body;
            return;
        }

        int streamNumber = StreamNumberOfDataChunk(chunkName);
        bool isKnownStream = streamNumber >= 0 && streamNumber < static_cast<int>(streams.size());
        if (!isKnownStream)
            return;

        MoviePiece piece{PlaceInFile(body), static_cast<std::uint32_t>(body.size())};
        streams[streamNumber].pieces.push_back(piece);
    }

    // The stream header: its kind, the codec, flags, priority, language, initial frames, then the time scale and the rate.
    void ReadStreamHeader(Formats::ByteView body)
    {
        Formats::ByteReader reader(body, DataDescription);

        AviStream stream;
        stream.kind = ReadFourCharacters(reader);
        reader.MoveTo(StreamHeaderTimeScalePlace);
        stream.timeScale = reader.ReadLittleEndianUint32();
        stream.rate = reader.ReadLittleEndianUint32();
        streams.push_back(stream);
    }

    std::uint64_t PlaceInFile(Formats::ByteView body) const
    {
        return static_cast<std::uint64_t>(body.data() - fileBytes.data());
    }

    Movie MakeMovie() const
    {
        Movie movie;
        for (const AviStream& stream : streams)
        {
            if (stream.kind == VideoStream && !movie.video)
                movie.video = MakeVideoTrack(stream, movie.durationSeconds);
            else if (stream.kind == SoundStream && !movie.sound)
                movie.sound = MakeSoundTrack(stream);
        }
        return movie;
    }

    // The format of a video stream is a BITMAPINFOHEADER: its size, the width, the height, planes, bits per pixel and the codec.
    static MovieVideoTrack MakeVideoTrack(const AviStream& stream, double& durationSeconds)
    {
        Formats::ByteReader reader(stream.format, DataDescription);
        reader.Skip(sizeof(std::uint32_t));

        MovieVideoTrack video;
        video.width = static_cast<int>(reader.ReadLittleEndianUint32());
        video.height = std::abs(static_cast<std::int32_t>(reader.ReadLittleEndianUint32()));
        reader.MoveTo(BitmapHeaderCodecPlace);
        video.codecName = ReadFourCharacters(reader);
        video.kind = video.codecName == MotionJpegCodec ? MovieVideoKind::MotionJpeg : MovieVideoKind::Unsupported;
        video.frames = stream.pieces;

        if (stream.rate == 0)
            throw Formats::FormatError("AVI movie: the video stream has no frame rate");

        double secondsPerFrame = static_cast<double>(stream.timeScale) / stream.rate;
        for (std::size_t frameNumber = 0; frameNumber < video.frames.size(); ++frameNumber)
            video.frameStartSeconds.push_back(frameNumber * secondsPerFrame);

        durationSeconds = video.frames.size() * secondsPerFrame;
        return video;
    }

    // The format of a sound stream is a WAVEFORMATEX: tag, channels, sample rate, bytes per second, block size, bits per sample,
    // and for IMA ADPCM the size of the extra data and the samples per block.
    static MovieSoundTrack MakeSoundTrack(const AviStream& stream)
    {
        Formats::ByteReader reader(stream.format, DataDescription);
        std::uint16_t formatTag = reader.ReadLittleEndianUint16();

        MovieSoundTrack sound;
        sound.channelCount = reader.ReadLittleEndianUint16();
        sound.sampleRate = static_cast<int>(reader.ReadLittleEndianUint32());
        reader.Skip(sizeof(std::uint32_t));
        sound.blockSize = reader.ReadLittleEndianUint16();
        sound.bitsPerSample = reader.ReadLittleEndianUint16();
        sound.codecName = std::format("WAVE format {:#06x}", formatTag);
        sound.pieces = stream.pieces;

        if (formatTag == PcmFormat && sound.bitsPerSample == EightBits)
            sound.kind = MovieSoundKind::Unsigned8Bit;
        else if (formatTag == PcmFormat && sound.bitsPerSample == SixteenBits)
            sound.kind = MovieSoundKind::LittleEndian16Bit;
        else if (formatTag == MuLawFormat)
            sound.kind = MovieSoundKind::MuLaw;
        else if (formatTag == ImaAdpcmFormat)
            sound.kind = MovieSoundKind::ImaAdpcm;

        if (sound.kind == MovieSoundKind::ImaAdpcm && sound.channelCount > 0)
        {
            int dataBytesPerChannel = sound.blockSize / sound.channelCount - ImaBlockHeaderSize;
            sound.samplesPerBlock = dataBytesPerChannel * ImaSamplesPerByte + 1;
        }
        return sound;
    }

    Formats::ByteView fileBytes;
    std::vector<AviStream> streams;
};

}

Movie ReadAviMovie(Formats::ByteView fileBytes)
{
    AviFileReader fileReader(fileBytes);
    return fileReader.Read();
}

}
