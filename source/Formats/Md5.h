#pragma once

#include "Formats/Bytes.h"

#include <array>
#include <cstddef>

namespace Formats
{

constexpr std::size_t Md5DigestSize = 16;
using Md5Digest = std::array<Byte, Md5DigestSize>;

// The MD5 digest of the data (RFC 1321). The System Menu keeps one in its save file iplsave.bin.
Md5Digest ComputeMd5(ByteView data);

}
