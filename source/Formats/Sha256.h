#pragma once

#include "Formats/Bytes.h"

#include <array>
#include <cstddef>

namespace Formats
{

constexpr std::size_t Sha256DigestSize = 32;
using Sha256Digest = std::array<Byte, Sha256DigestSize>;

Sha256Digest ComputeSha256(ByteView data);

}
