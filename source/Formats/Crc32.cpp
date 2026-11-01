#include "Formats/Crc32.h"

namespace Formats
{

namespace
{

constexpr std::uint32_t Polynomial = 0xEDB88320;
constexpr std::uint32_t AllOnes = 0xFFFFFFFF;

}

std::uint32_t Crc32(ByteView bytes)
{
    std::uint32_t checksum = AllOnes;
    for (Byte value : bytes)
    {
        checksum ^= value;
        for (int bit = 0; bit < BitsPerByte; ++bit)
        {
            bool lowestBitIsSet = (checksum & 1) != 0;
            checksum >>= 1;
            if (lowestBitIsSet)
                checksum ^= Polynomial;
        }
    }
    return ~checksum;
}

}
