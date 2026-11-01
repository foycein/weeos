#include "Formats/Crc32.h"

namespace Formats
{

namespace
{

constexpr std::uint32_t Polynomial = 0xEDB88320;
constexpr std::uint32_t AllOnes = 0xFFFFFFFF;

}

std::uint32_t ComputeCrc32(ByteView data)
{
    std::uint32_t crc = AllOnes;
    for (Byte byte : data)
    {
        crc ^= byte;
        for (int bit = 0; bit < BitsPerByte; ++bit)
        {
            bool lowestBitIsSet = (crc & 1) != 0;
            crc >>= 1;
            if (lowestBitIsSet)
                crc ^= Polynomial;
        }
    }
    return crc ^ AllOnes;
}

}
