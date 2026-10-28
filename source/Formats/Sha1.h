#pragma once

#include "Formats/Bytes.h"

#include <array>
#include <cstddef>

namespace Formats
{

constexpr std::size_t Sha1DigestSize = 20;
using Sha1Digest = std::array<Byte, Sha1DigestSize>;

Sha1Digest ComputeSha1(ByteView data);

}
