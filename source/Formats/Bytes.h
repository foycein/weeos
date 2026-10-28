#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace Formats
{

using Byte = std::uint8_t;
using Bytes = std::vector<Byte>;
using ByteView = std::span<const Byte>;

}
