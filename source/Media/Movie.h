#pragma once

#include "Formats/Bytes.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Media
{

// A piece of a movie file: one video frame, one chunk of sound, or one compressed sound frame.
struct MoviePiece
{
    std::uint64_t place = 0;
    std::uint32_t size = 0;
};

enum class MovieVideoKind
{
    // Every frame is a JPEG picture with its markers (Motion-JPEG A, QuickTime "jpeg", AVI "MJPG"), one picture per field.
    MotionJpeg,

    // QuickTime "mjpb": every field starts with a table of places instead of markers and its data has no byte stuffing.
    MotionJpegB,

    Unsupported,
};

enum class MovieSoundKind
{
    Unsigned8Bit,
    Signed8Bit,
    BigEndian16Bit,
    LittleEndian16Bit,
    MuLaw,
    ALaw,

    // IMA ADPCM in the blocks of a WAV or AVI file (format 0x11): 4 bits a sample.
    ImaAdpcm,

    // AAC in an MP4 file: the pieces are AAC frames, the codec setup is the AudioSpecificConfig of the "esds" box.
    Aac,

    Unsupported,
};

struct MovieVideoTrack
{
    MovieVideoKind kind = MovieVideoKind::Unsupported;

    // The four characters of the codec as the file names it, for the messages about what is not supported.
    std::string codecName;

    int width = 0;
    int height = 0;

    // 2 when every frame holds two fields of half the height (interlaced), one after the other: first the field of the even lines.
    int fieldCount = 1;

    std::vector<MoviePiece> frames;

    // When each frame starts, in seconds from the start of the movie.
    std::vector<double> frameStartSeconds;
};

struct MovieSoundTrack
{
    MovieSoundKind kind = MovieSoundKind::Unsupported;
    std::string codecName;
    int sampleRate = 0;
    int channelCount = 0;
    int bitsPerSample = 0;

    // For IMA ADPCM: the bytes of one block (all channels) and the samples of one channel in it.
    int blockSize = 0;
    int samplesPerBlock = 0;

    // For AAC.
    Formats::Bytes decoderSetup;

    std::vector<MoviePiece> pieces;
};

struct Movie
{
    std::optional<MovieVideoTrack> video;
    std::optional<MovieSoundTrack> sound;
    double durationSeconds = 0;
};

}
