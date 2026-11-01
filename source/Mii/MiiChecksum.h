#pragma once

#include "Formats/Bytes.h"

#include <cstdint>

namespace Mii
{

// The checksum of the Mii database and of the Mii block of a Wii Remote: CRC-16 with the polynomial 0x1021 and the start value 0
// (the variant called XMODEM). It is stored big endian right after the bytes it covers.
std::uint16_t CalculateMiiChecksum(Formats::ByteView bytes);

}
