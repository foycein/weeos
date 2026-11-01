#pragma once

#include "Formats/Bytes.h"

#include <cstdint>

namespace Formats
{

// The CRC-32 of zlib and PNG: the reversed polynomial 0xEDB88320, started at all ones and inverted at the end.
std::uint32_t Crc32(ByteView bytes);

}
