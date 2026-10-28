#include "Formats/RsaSignature.h"

#include "Formats/BigNumber.h"


namespace Formats
{

namespace
{

constexpr Byte BlockTypeSignature = 0x01;
constexpr Byte PaddingByte = 0xFF;
constexpr std::size_t MinimumPaddingSize = 8;

// The name of the hash algorithm SHA-1 in front of the hash, as PKCS #1 writes it.
constexpr Byte Sha1AlgorithmName[] = {0x30, 0x21, 0x30, 0x09, 0x06, 0x05, 0x2B, 0x0E, 0x03, 0x02, 0x1A, 0x05, 0x00, 0x04, 0x14};

// The block that a correct signature of the hash gives back: 00 01 FF ... FF 00, the algorithm name and the hash.
Bytes MakeExpectedBlock(std::size_t blockSize, const Sha1Digest& hash)
{
    std::size_t trailerSize = 1 + sizeof(Sha1AlgorithmName) + Sha1DigestSize;
    std::size_t paddingSize = blockSize - 2 - trailerSize;

    Bytes block;
    block.push_back(0x00);
    block.push_back(BlockTypeSignature);
    block.insert(block.end(), paddingSize, PaddingByte);
    block.push_back(0x00);
    block.insert(block.end(), std::begin(Sha1AlgorithmName), std::end(Sha1AlgorithmName));
    block.insert(block.end(), hash.begin(), hash.end());
    return block;
}

}

bool VerifyRsaSha1Signature(ByteView modulus, std::uint32_t publicExponent, ByteView signature, const Sha1Digest& hash)
{
    std::size_t smallestBlockSize = 2 + MinimumPaddingSize + 1 + sizeof(Sha1AlgorithmName) + Sha1DigestSize;
    if (modulus.size() < smallestBlockSize || signature.size() != modulus.size())
        return false;

    BigNumber modulusNumber = BigNumber::FromBigEndianBytes(modulus);
    BigNumber signatureNumber = BigNumber::FromBigEndianBytes(signature);
    if (signatureNumber >= modulusNumber)
        return false;

    BigNumber block = signatureNumber.PowerModulo(BigNumber(publicExponent), modulusNumber);
    Bytes blockBytes = block.ToBigEndianBytes(modulus.size());
    return blockBytes == MakeExpectedBlock(modulus.size(), hash);
}

}
