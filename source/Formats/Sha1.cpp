#include "Formats/Sha1.h"

#include "Formats/ByteReader.h"

#include <bit>
#include <cstdint>

namespace Formats
{

namespace
{

constexpr std::size_t BlockSize = 64;
constexpr std::size_t LengthFieldSize = 8;
constexpr std::size_t WordSize = 4;
constexpr int WordsPerBlock = BlockSize / WordSize;
constexpr int RoundCount = 80;
constexpr int RoundsPerStage = 20;

constexpr std::uint32_t FirstStageConstant = 0x5A827999;
constexpr std::uint32_t SecondStageConstant = 0x6ED9EBA1;
constexpr std::uint32_t ThirdStageConstant = 0x8F1BBCDC;
constexpr std::uint32_t FourthStageConstant = 0xCA62C1D6;

constexpr Byte EndOfDataMarker = 0x80;

constexpr std::size_t StateWordCount = 5;
using Sha1State = std::array<std::uint32_t, StateWordCount>;

constexpr Sha1State InitialState = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};

// The 16 words of the block come first. Each of the other words is made from four earlier ones.
// The distances 3, 8, 14 and 16 and the turns by 1, 5 and 30 bits below are fixed by the SHA-1 standard.
std::array<std::uint32_t, RoundCount> ExpandBlockIntoWords(ByteView block)
{
    std::array<std::uint32_t, RoundCount> words;

    ByteReader reader(block, "SHA-1 block");
    for (int index = 0; index < WordsPerBlock; ++index)
        words[index] = reader.ReadUint32();

    for (int index = WordsPerBlock; index < RoundCount; ++index)
        words[index] = std::rotl(words[index - 3] ^ words[index - 8] ^ words[index - 14] ^ words[index - 16], 1);

    return words;
}

std::uint32_t MixValues(int round, std::uint32_t b, std::uint32_t c, std::uint32_t d)
{
    if (round < RoundsPerStage)
        return (b & c) | (~b & d);

    if (round < 2 * RoundsPerStage)
        return b ^ c ^ d;

    if (round < 3 * RoundsPerStage)
        return (b & c) | (b & d) | (c & d);

    return b ^ c ^ d;
}

std::uint32_t StageConstant(int round)
{
    if (round < RoundsPerStage)
        return FirstStageConstant;

    if (round < 2 * RoundsPerStage)
        return SecondStageConstant;

    if (round < 3 * RoundsPerStage)
        return ThirdStageConstant;

    return FourthStageConstant;
}

void ProcessBlock(ByteView block, Sha1State& state)
{
    std::array<std::uint32_t, RoundCount> words = ExpandBlockIntoWords(block);

    // The SHA-1 standard calls the five working values a, b, c, d and e.
    std::uint32_t a = state[0];
    std::uint32_t b = state[1];
    std::uint32_t c = state[2];
    std::uint32_t d = state[3];
    std::uint32_t e = state[4];

    for (int round = 0; round < RoundCount; ++round)
    {
        std::uint32_t next = std::rotl(a, 5) + MixValues(round, b, c, d) + e + StageConstant(round) + words[round];
        e = d;
        d = c;
        c = std::rotl(b, 30);
        b = a;
        a = next;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
}

// The data is padded with a marker, zeros and the length in bits, so that it ends on a block boundary.
Bytes PadLastBytes(ByteView lastBytes, std::uint64_t totalByteCount)
{
    Bytes padded(lastBytes.begin(), lastBytes.end());
    padded.push_back(EndOfDataMarker);

    while (padded.size() % BlockSize != BlockSize - LengthFieldSize)
        padded.push_back(0);

    std::uint64_t totalBitCount = totalByteCount * BitsPerByte;
    for (std::size_t byteIndex = LengthFieldSize; byteIndex > 0; --byteIndex)
        padded.push_back(static_cast<Byte>(totalBitCount >> ((byteIndex - 1) * BitsPerByte)));

    return padded;
}

Sha1Digest StateToDigest(const Sha1State& state)
{
    Sha1Digest digest;
    for (std::size_t wordIndex = 0; wordIndex < state.size(); ++wordIndex)
    {
        for (std::size_t byteIndex = 0; byteIndex < WordSize; ++byteIndex)
        {
            int shift = static_cast<int>(BitsPerByte * (WordSize - 1 - byteIndex));
            digest[wordIndex * WordSize + byteIndex] = static_cast<Byte>(state[wordIndex] >> shift);
        }
    }
    return digest;
}

}

Sha1Digest ComputeSha1(ByteView data)
{
    Sha1State state = InitialState;

    std::size_t wholeBlockBytes = data.size() - data.size() % BlockSize;
    for (std::size_t offset = 0; offset < wholeBlockBytes; offset += BlockSize)
        ProcessBlock(data.subspan(offset, BlockSize), state);

    Bytes padded = PadLastBytes(data.subspan(wholeBlockBytes), data.size());
    for (std::size_t offset = 0; offset < padded.size(); offset += BlockSize)
        ProcessBlock(ByteView(padded).subspan(offset, BlockSize), state);

    return StateToDigest(state);
}

}
