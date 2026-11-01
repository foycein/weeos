#pragma once

#include "Formats/Bytes.h"

#include <cstdint>

namespace Formats
{

// The CRC-32 of zip and PNG (polynomial 0xEDB88320, starts with all ones, inverted at the end). The System Menu checks a
// Message Board record with it (0x814936C0).
std::uint32_t ComputeCrc32(ByteView data);

}
