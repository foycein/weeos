#include "Formats/HmacSha1.h"

#include <algorithm>

namespace Formats
{

namespace
{

constexpr std::size_t BlockSize = 64;
constexpr Byte InnerPadding = 0x36;
constexpr Byte OuterPadding = 0x5C;

// The key, filled up with zeros to one block of the hash.
Bytes MakeBlockKey(ByteView key)
{
    Bytes blockKey(BlockSize, Byte(0));

    if (key.size() > BlockSize)
    {
        Sha1Digest hashedKey = ComputeSha1(key);
        std::copy(hashedKey.begin(), hashedKey.end(), blockKey.begin());
        return blockKey;
    }

    std::copy(key.begin(), key.end(), blockKey.begin());
    return blockKey;
}

// The block key with every byte combined with the padding byte, followed by the data.
Bytes PaddedKeyAndData(const Bytes& blockKey, Byte padding, ByteView data)
{
    Bytes bytes;
    bytes.reserve(BlockSize + data.size());

    for (Byte keyByte : blockKey)
        bytes.push_back(static_cast<Byte>(keyByte ^ padding));

    bytes.insert(bytes.end(), data.begin(), data.end());
    return bytes;
}

}

Sha1Digest ComputeHmacSha1(ByteView key, ByteView message)
{
    Bytes blockKey = MakeBlockKey(key);

    Sha1Digest innerDigest = ComputeSha1(PaddedKeyAndData(blockKey, InnerPadding, message));
    return ComputeSha1(PaddedKeyAndData(blockKey, OuterPadding, innerDigest));
}

}
