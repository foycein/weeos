#include "Media/MovieSound.h"

#include "Formats/ByteReader.h"
#include "Formats/FormatError.h"

#include <algorithm>
#include <array>
#include <limits>

namespace Media
{

namespace
{

constexpr int BitsPerByte = 8;
constexpr int UnsignedMiddle = 128;

// G.711: a sign bit, three bits of exponent and four of mantissa.
constexpr int SignBit = 0x80;
constexpr int ExponentShift = 4;
constexpr int ExponentMask = 0x07;
constexpr int MantissaMask = 0x0F;
constexpr int MuLawBias = 0x84;
constexpr int MuLawMantissaShift = 3;
constexpr Formats::Byte ALawEvenBitsInverted = 0x55;
constexpr int ALawMantissaShift = 4;
constexpr int ALawHalfStep = 8;
constexpr int ALawSegmentStart = 0x108;

// IMA ADPCM (the IMA Digital Audio Focus and Technical Working Groups, 1992): the step sizes and how a code moves the index.
constexpr std::array<int, 89> ImaStepSizes = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143,
    157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552,
    1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487,
    12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767};
constexpr std::array<int, 8> ImaIndexChanges = {-1, -1, -1, -1, 2, 4, 6, 8};
constexpr int ImaHeaderSizePerChannel = 4;
constexpr int ImaBytesPerGroup = 4;
constexpr int ImaSamplesPerGroup = 8;
constexpr int ImaNegativeBit = 8;
constexpr int ImaFourStepsBit = 4;
constexpr int ImaTwoStepsBit = 2;
constexpr int ImaOneStepBit = 1;
constexpr int ImaMagnitudeMask = 7;
constexpr int NibbleBits = 4;
constexpr int NibbleMask = 0x0F;

std::int16_t ClampToSample(int value)
{
    int clamped = std::clamp(value, static_cast<int>(std::numeric_limits<std::int16_t>::min()),
        static_cast<int>(std::numeric_limits<std::int16_t>::max()));
    return static_cast<std::int16_t>(clamped);
}

Formats::Bytes JoinPieces(const MovieSoundTrack& sound, Formats::ByteView fileBytes)
{
    Formats::Bytes joined;
    for (const MoviePiece& piece : sound.pieces)
    {
        if (!Formats::IsRangeInside(piece.place, piece.size, fileBytes.size()))
            throw Formats::FormatError("movie sound: a piece of sound lies outside the file");

        Formats::ByteView bytes = fileBytes.subspan(static_cast<std::size_t>(piece.place), piece.size);
        joined.insert(joined.end(), bytes.begin(), bytes.end());
    }
    return joined;
}

std::int16_t SixteenBitsOf(Formats::Byte first, Formats::Byte second, bool isBigEndian)
{
    int high = isBigEndian ? first : second;
    int low = isBigEndian ? second : first;
    return static_cast<std::int16_t>((high << BitsPerByte) | low);
}

void DecodeSixteenBits(const Formats::Bytes& bytes, bool isBigEndian, std::vector<std::int16_t>& samples)
{
    for (std::size_t place = 0; place + 1 < bytes.size(); place += 2)
        samples.push_back(SixteenBitsOf(bytes[place], bytes[place + 1], isBigEndian));
}

struct ImaChannelState
{
    int predictor = 0;
    int stepIndex = 0;
};

std::int16_t DecodeImaCode(ImaChannelState& state, int code)
{
    int step = ImaStepSizes[state.stepIndex];
    int difference = step >> 3;
    if ((code & ImaFourStepsBit) != 0)
        difference += step;
    if ((code & ImaTwoStepsBit) != 0)
        difference += step >> 1;
    if ((code & ImaOneStepBit) != 0)
        difference += step >> 2;

    if ((code & ImaNegativeBit) != 0)
        state.predictor -= difference;
    else
        state.predictor += difference;

    state.predictor = ClampToSample(state.predictor);
    state.stepIndex = std::clamp(state.stepIndex + ImaIndexChanges[code & ImaMagnitudeMask], 0, static_cast<int>(ImaStepSizes.size()) - 1);
    return static_cast<std::int16_t>(state.predictor);
}

}

std::int16_t DecodeMuLaw(Formats::Byte value)
{
    int inverted = static_cast<Formats::Byte>(~value);
    int exponent = (inverted >> ExponentShift) & ExponentMask;
    int mantissa = inverted & MantissaMask;
    int magnitude = (((mantissa << MuLawMantissaShift) + MuLawBias) << exponent) - MuLawBias;
    bool isNegative = (inverted & SignBit) != 0;
    return static_cast<std::int16_t>(isNegative ? -magnitude : magnitude);
}

std::int16_t DecodeALaw(Formats::Byte value)
{
    int flipped = value ^ ALawEvenBitsInverted;
    int exponent = (flipped >> ExponentShift) & ExponentMask;
    int mantissa = flipped & MantissaMask;

    int magnitude = (mantissa << ALawMantissaShift) + ALawHalfStep;
    if (exponent > 0)
        magnitude = ((mantissa << ALawMantissaShift) + ALawSegmentStart) << (exponent - 1);

    bool isPositive = (flipped & SignBit) != 0;
    return static_cast<std::int16_t>(isPositive ? magnitude : -magnitude);
}

void DecodeImaAdpcmBlock(Formats::ByteView block, int channelCount, int samplesPerBlock, std::vector<std::int16_t>& samples)
{
    std::size_t headerSize = static_cast<std::size_t>(ImaHeaderSizePerChannel) * channelCount;
    if (channelCount <= 0 || samplesPerBlock <= 0 || block.size() < headerSize)
        return;

    std::vector<ImaChannelState> states(channelCount);
    std::vector<std::int16_t> blockSamples(static_cast<std::size_t>(samplesPerBlock) * channelCount, 0);
    for (int channel = 0; channel < channelCount; ++channel)
    {
        Formats::ByteView header = block.subspan(static_cast<std::size_t>(channel) * ImaHeaderSizePerChannel);
        states[channel].predictor = SixteenBitsOf(header[0], header[1], false);
        states[channel].stepIndex = std::clamp(static_cast<int>(header[2]), 0, static_cast<int>(ImaStepSizes.size()) - 1);
        blockSamples[channel] = static_cast<std::int16_t>(states[channel].predictor);
    }

    Formats::ByteView data = block.subspan(headerSize);
    int codedSamples = samplesPerBlock - 1;
    int groupCount = (codedSamples + ImaSamplesPerGroup - 1) / ImaSamplesPerGroup;
    for (int group = 0; group < groupCount; ++group)
    {
        for (int channel = 0; channel < channelCount; ++channel)
        {
            std::size_t groupStart = (static_cast<std::size_t>(group) * channelCount + channel) * ImaBytesPerGroup;
            for (int byteInGroup = 0; byteInGroup < ImaBytesPerGroup && groupStart + byteInGroup < data.size(); ++byteInGroup)
            {
                Formats::Byte codes = data[groupStart + byteInGroup];
                int firstSample = 1 + group * ImaSamplesPerGroup + byteInGroup * 2;
                int lowCode = codes & NibbleMask;
                int highCode = codes >> NibbleBits;

                if (firstSample < samplesPerBlock)
                    blockSamples[static_cast<std::size_t>(firstSample) * channelCount + channel] = DecodeImaCode(states[channel], lowCode);
                if (firstSample + 1 < samplesPerBlock)
                    blockSamples[static_cast<std::size_t>(firstSample + 1) * channelCount + channel] = DecodeImaCode(states[channel], highCode);
            }
        }
    }

    samples.insert(samples.end(), blockSamples.begin(), blockSamples.end());
}

PcmSound DecodeMovieSound(const MovieSoundTrack& sound, Formats::ByteView fileBytes)
{
    if (sound.kind == MovieSoundKind::Unsupported || sound.kind == MovieSoundKind::Aac)
        throw Formats::FormatError("movie sound: the codec " + sound.codecName + " is not decoded here");

    PcmSound pcm;
    pcm.sampleRate = sound.sampleRate;
    pcm.channelCount = sound.channelCount;
    Formats::Bytes bytes = JoinPieces(sound, fileBytes);

    switch (sound.kind)
    {
    case MovieSoundKind::Unsigned8Bit:
        for (Formats::Byte value : bytes)
            pcm.samples.push_back(static_cast<std::int16_t>((value - UnsignedMiddle) << BitsPerByte));
        break;

    case MovieSoundKind::Signed8Bit:
        for (Formats::Byte value : bytes)
            pcm.samples.push_back(static_cast<std::int16_t>(static_cast<std::int8_t>(value) << BitsPerByte));
        break;

    case MovieSoundKind::BigEndian16Bit:
        DecodeSixteenBits(bytes, true, pcm.samples);
        break;

    case MovieSoundKind::LittleEndian16Bit:
        DecodeSixteenBits(bytes, false, pcm.samples);
        break;

    case MovieSoundKind::MuLaw:
        for (Formats::Byte value : bytes)
            pcm.samples.push_back(DecodeMuLaw(value));
        break;

    case MovieSoundKind::ALaw:
        for (Formats::Byte value : bytes)
            pcm.samples.push_back(DecodeALaw(value));
        break;

    case MovieSoundKind::ImaAdpcm:
        for (std::size_t place = 0; sound.blockSize > 0 && place < bytes.size(); place += sound.blockSize)
        {
            std::size_t size = std::min<std::size_t>(sound.blockSize, bytes.size() - place);
            DecodeImaAdpcmBlock(Formats::ByteView(bytes).subspan(place, size), sound.channelCount, sound.samplesPerBlock, pcm.samples);
        }
        break;

    default:
        break;
    }
    return pcm;
}

}
