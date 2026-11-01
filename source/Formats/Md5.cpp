#include "Formats/Md5.h"

#include <bit>
#include <cmath>
#include <cstdint>

namespace Formats
{

namespace
{

constexpr std::size_t BlockSize = 64;
constexpr std::size_t LengthFieldSize = 8;
constexpr std::size_t WordSize = 4;
constexpr int WordsPerBlock = BlockSize / WordSize;
constexpr int RoundCount = 64;
constexpr int RoundsPerStage = 16;
constexpr int ShiftsPerStage = 4;

constexpr Byte EndOfDataMarker = 0x80;

using Md5State = std::array<std::uint32_t, 4>;

constexpr Md5State InitialState = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476};

// How far each of the four stages turns a value, in the order of the rounds of the stage.
constexpr std::array<int, 4 * ShiftsPerStage> ShiftAmounts = {7, 12, 17, 22, 5, 9, 14, 20, 4, 11, 16, 23, 6, 10, 15, 21};

// The constant of round n is the whole part of 2^32 times the absolute value of sin(n + 1), as RFC 1321 says.
std::uint32_t RoundConstant(int round)
{
    constexpr double TwoToThe32 = 4294967296.0;
    return static_cast<std::uint32_t>(std::fabs(std::sin(round + 1.0)) * TwoToThe32);
}

// The block is read as 16 words with the least significant byte first.
std::array<std::uint32_t, WordsPerBlock> ReadWordsOfBlock(ByteView block)
{
    std::array<std::uint32_t, WordsPerBlock> words;
    for (int wordIndex = 0; wordIndex < WordsPerBlock; ++wordIndex)
    {
        std::uint32_t word = 0;
        for (std::size_t byteIndex = WordSize; byteIndex > 0; --byteIndex)
            word = (word << BitsPerByte) | block[wordIndex * WordSize + byteIndex - 1];

        words[wordIndex] = word;
    }
    return words;
}

std::uint32_t MixValues(int stage, std::uint32_t b, std::uint32_t c, std::uint32_t d)
{
    if (stage == 0)
        return (b & c) | (~b & d);

    if (stage == 1)
        return (d & b) | (~d & c);

    if (stage == 2)
        return b ^ c ^ d;

    return c ^ (b | ~d);
}

// Which of the 16 words of the block a round reads. Every stage walks through the words in a different order.
int WordIndexOfRound(int stage, int roundInStage)
{
    if (stage == 0)
        return roundInStage;

    if (stage == 1)
        return (5 * roundInStage + 1) % WordsPerBlock;

    if (stage == 2)
        return (3 * roundInStage + 5) % WordsPerBlock;

    return (7 * roundInStage) % WordsPerBlock;
}

void ProcessBlock(ByteView block, Md5State& state)
{
    std::array<std::uint32_t, WordsPerBlock> words = ReadWordsOfBlock(block);

    // RFC 1321 calls the four working values a, b, c and d.
    std::uint32_t a = state[0];
    std::uint32_t b = state[1];
    std::uint32_t c = state[2];
    std::uint32_t d = state[3];

    for (int round = 0; round < RoundCount; ++round)
    {
        int stage = round / RoundsPerStage;
        int roundInStage = round % RoundsPerStage;

        std::uint32_t mixed = MixValues(stage, b, c, d) + a + RoundConstant(round) + words[WordIndexOfRound(stage, roundInStage)];
        int shift = ShiftAmounts[stage * ShiftsPerStage + roundInStage % ShiftsPerStage];

        a = d;
        d = c;
        c = b;
        b = b + std::rotl(mixed, shift);
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
}

// The data is padded with a marker, zeros and the length in bits, which MD5 writes with the least significant byte first.
Bytes PadLastBytes(ByteView lastBytes, std::uint64_t totalByteCount)
{
    Bytes padded(lastBytes.begin(), lastBytes.end());
    padded.push_back(EndOfDataMarker);

    while (padded.size() % BlockSize != BlockSize - LengthFieldSize)
        padded.push_back(0);

    std::uint64_t totalBitCount = totalByteCount * BitsPerByte;
    for (std::size_t byteIndex = 0; byteIndex < LengthFieldSize; ++byteIndex)
        padded.push_back(static_cast<Byte>(totalBitCount >> (byteIndex * BitsPerByte)));

    return padded;
}

Md5Digest StateToDigest(const Md5State& state)
{
    Md5Digest digest;
    for (std::size_t wordIndex = 0; wordIndex < state.size(); ++wordIndex)
    {
        for (std::size_t byteIndex = 0; byteIndex < WordSize; ++byteIndex)
            digest[wordIndex * WordSize + byteIndex] = static_cast<Byte>(state[wordIndex] >> (BitsPerByte * byteIndex));
    }
    return digest;
}

}

Md5Digest ComputeMd5(ByteView data)
{
    Md5State state = InitialState;

    std::size_t wholeBlockBytes = data.size() - data.size() % BlockSize;
    for (std::size_t offset = 0; offset < wholeBlockBytes; offset += BlockSize)
        ProcessBlock(data.subspan(offset, BlockSize), state);

    Bytes paddedEnd = PadLastBytes(data.subspan(wholeBlockBytes), data.size());
    for (std::size_t offset = 0; offset < paddedEnd.size(); offset += BlockSize)
        ProcessBlock(ByteView(paddedEnd).subspan(offset, BlockSize), state);

    return StateToDigest(state);
}

}
