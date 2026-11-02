#include "Formats/Sha256.h"

#include <algorithm>
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
constexpr int RoundCount = 64;

constexpr Byte EndOfDataMarker = 0x80;

constexpr std::size_t StateWordCount = 8;
using Sha256State = std::array<std::uint32_t, StateWordCount>;

// The first 32 bits of the fractional parts of the square roots of the first eight primes, as the SHA-256 standard gives them.
constexpr Sha256State InitialState = {
    0x6A09E667, 0xBB67AE85, 0x3C6EF372, 0xA54FF53A, 0x510E527F, 0x9B05688C, 0x1F83D9AB, 0x5BE0CD19,
};

// The first 32 bits of the fractional parts of the cube roots of the first 64 primes, one for each round.
constexpr std::array<std::uint32_t, RoundCount> RoundConstants = {
    0x428A2F98, 0x71374491, 0xB5C0FBCF, 0xE9B5DBA5, 0x3956C25B, 0x59F111F1, 0x923F82A4, 0xAB1C5ED5,
    0xD807AA98, 0x12835B01, 0x243185BE, 0x550C7DC3, 0x72BE5D74, 0x80DEB1FE, 0x9BDC06A7, 0xC19BF174,
    0xE49B69C1, 0xEFBE4786, 0x0FC19DC6, 0x240CA1CC, 0x2DE92C6F, 0x4A7484AA, 0x5CB0A9DC, 0x76F988DA,
    0x983E5152, 0xA831C66D, 0xB00327C8, 0xBF597FC7, 0xC6E00BF3, 0xD5A79147, 0x06CA6351, 0x14292967,
    0x27B70A85, 0x2E1B2138, 0x4D2C6DFC, 0x53380D13, 0x650A7354, 0x766A0ABB, 0x81C2C92E, 0x92722C85,
    0xA2BFE8A1, 0xA81A664B, 0xC24B8B70, 0xC76C51A3, 0xD192E819, 0xD6990624, 0xF40E3585, 0x106AA070,
    0x19A4C116, 0x1E376C08, 0x2748774C, 0x34B0BCB5, 0x391C0CB3, 0x4ED8AA4A, 0x5B9CCA4F, 0x682E6FF3,
    0x748F82EE, 0x78A5636F, 0x84C87814, 0x8CC70208, 0x90BEFFFA, 0xA4506CEB, 0xBEF9A3F7, 0xC67178F2,
};

// The four mixing functions of the standard (sigma 0, sigma 1, Sigma 0 and Sigma 1). Their turns and shifts are fixed by it.
std::uint32_t MixOlderWord(std::uint32_t word)
{
    return std::rotr(word, 7) ^ std::rotr(word, 18) ^ (word >> 3);
}

std::uint32_t MixRecentWord(std::uint32_t word)
{
    return std::rotr(word, 17) ^ std::rotr(word, 19) ^ (word >> 10);
}

std::uint32_t MixFirstValue(std::uint32_t a)
{
    return std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22);
}

std::uint32_t MixFifthValue(std::uint32_t e)
{
    return std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25);
}

// Each bit comes from f where e has it set, else from g.
std::uint32_t Choose(std::uint32_t e, std::uint32_t f, std::uint32_t g)
{
    return (e & f) ^ (~e & g);
}

// Each bit is the one that at least two of the three values have.
std::uint32_t Majority(std::uint32_t a, std::uint32_t b, std::uint32_t c)
{
    return (a & b) ^ (a & c) ^ (b & c);
}

// The four bytes at the offset, the most significant first. (ByteReader is not used here: it is not optimized, and this runs millions
// of times when the keys of a console are found.)
std::uint32_t ReadBigEndianWord(ByteView block, std::size_t offset)
{
    std::uint32_t word = 0;
    for (std::size_t index = 0; index < WordSize; ++index)
        word = (word << BitsPerByte) | block[offset + index];

    return word;
}

// The 16 words of the block come first. Each of the other words is made from four earlier ones.
std::array<std::uint32_t, RoundCount> ExpandBlockIntoWords(ByteView block)
{
    std::array<std::uint32_t, RoundCount> words;

    for (int index = 0; index < WordsPerBlock; ++index)
        words[index] = ReadBigEndianWord(block, index * WordSize);

    for (int index = WordsPerBlock; index < RoundCount; ++index)
        words[index] = MixRecentWord(words[index - 2]) + words[index - 7] + MixOlderWord(words[index - 15]) + words[index - 16];

    return words;
}

void ProcessBlock(ByteView block, Sha256State& state)
{
    std::array<std::uint32_t, RoundCount> words = ExpandBlockIntoWords(block);

    // The SHA-256 standard calls the eight working values a to h.
    std::uint32_t a = state[0];
    std::uint32_t b = state[1];
    std::uint32_t c = state[2];
    std::uint32_t d = state[3];
    std::uint32_t e = state[4];
    std::uint32_t f = state[5];
    std::uint32_t g = state[6];
    std::uint32_t h = state[7];

    for (int round = 0; round < RoundCount; ++round)
    {
        std::uint32_t first = h + MixFifthValue(e) + Choose(e, f, g) + RoundConstants[round] + words[round];
        std::uint32_t second = MixFirstValue(a) + Majority(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + first;
        d = c;
        c = b;
        b = a;
        a = first + second;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

// The bytes after the last whole block, with their padding, fill one or two blocks. They are padded in place, without a new allocation,
// because the keys of a console are found by hashing millions of pieces of 16 bytes (Ios::FindSharedKeys).
using PaddedBlocks = std::array<Byte, 2 * BlockSize>;

// The data is padded with a marker, zeros and the length in bits, so that it ends on a block boundary. Gives back the padded size.
std::size_t PadLastBytes(ByteView lastBytes, std::uint64_t totalByteCount, PaddedBlocks& padded)
{
    padded.fill(0);
    std::copy(lastBytes.begin(), lastBytes.end(), padded.begin());
    padded[lastBytes.size()] = EndOfDataMarker;

    bool lengthFitsBehindTheMarker = lastBytes.size() < BlockSize - LengthFieldSize;
    std::size_t paddedSize = lengthFitsBehindTheMarker ? BlockSize : 2 * BlockSize;

    std::uint64_t totalBitCount = totalByteCount * BitsPerByte;
    for (std::size_t byteIndex = 0; byteIndex < LengthFieldSize; ++byteIndex)
        padded[paddedSize - 1 - byteIndex] = static_cast<Byte>(totalBitCount >> (byteIndex * BitsPerByte));

    return paddedSize;
}

Sha256Digest StateToDigest(const Sha256State& state)
{
    Sha256Digest digest;
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

Sha256Digest ComputeSha256(ByteView data)
{
    Sha256State state = InitialState;

    std::size_t wholeBlockBytes = data.size() - data.size() % BlockSize;
    for (std::size_t offset = 0; offset < wholeBlockBytes; offset += BlockSize)
        ProcessBlock(data.subspan(offset, BlockSize), state);

    PaddedBlocks padded;
    std::size_t paddedSize = PadLastBytes(data.subspan(wholeBlockBytes), data.size(), padded);
    for (std::size_t offset = 0; offset < paddedSize; offset += BlockSize)
        ProcessBlock(ByteView(padded).subspan(offset, BlockSize), state);

    return StateToDigest(state);
}

}
