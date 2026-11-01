#include "Mii/MiiChecksum.h"

namespace Mii
{

namespace
{

constexpr std::uint16_t Polynomial = 0x1021;
constexpr std::uint16_t TopBit = 0x8000;

}

std::uint16_t CalculateMiiChecksum(Formats::ByteView bytes)
{
    std::uint16_t checksum = 0;

    for (Formats::Byte byte : bytes)
    {
        checksum ^= static_cast<std::uint16_t>(byte << Formats::BitsPerByte);

        for (int bit = 0; bit < Formats::BitsPerByte; ++bit)
        {
            bool topBitIsSet = (checksum & TopBit) != 0;
            checksum = static_cast<std::uint16_t>(checksum << 1);
            if (topBitIsSet)
                checksum ^= Polynomial;
        }
    }

    return checksum;
}

}
