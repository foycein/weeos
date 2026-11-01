#include "Mii/MiiCreateId.h"

#include "Mii/MiiRecord.h"

namespace Mii
{

namespace
{

constexpr std::chrono::sys_days FirstDayOfMiiTime = std::chrono::year(2006) / std::chrono::January / 1;
constexpr std::int64_t SecondsPerStep = 4;
constexpr std::uint32_t StepNumberMask = 0x0FFFFFFF;

// Nintendo's own consoles: their system ids keep the top bit of the first byte.
constexpr std::array<Formats::Byte, 3> NintendoAddressStart = {0x00, 0x17, 0xAB};
constexpr std::uint32_t SevenBitMask = 0x7F;

// The system id ends with the last three bytes of the address.
constexpr std::size_t FirstCopiedAddressByte = 3;

}

std::uint32_t MakeMiiId(std::chrono::sys_seconds creationTime)
{
    std::int64_t secondsSinceStart = (creationTime - FirstDayOfMiiTime).count();
    auto stepNumber = static_cast<std::uint32_t>(secondsSinceStart / SecondsPerStep);
    return (stepNumber & StepNumberMask) | NormalMiiFlag;
}

std::uint32_t MakeSystemId(const std::array<Formats::Byte, MacAddressSize>& macAddress)
{
    std::uint32_t firstByte = (macAddress[0] + macAddress[1] + macAddress[2]) & Formats::ByteMask;

    bool isNintendoAddress = macAddress[0] == NintendoAddressStart[0] && macAddress[1] == NintendoAddressStart[1]
        && macAddress[2] == NintendoAddressStart[2];
    if (!isNintendoAddress)
        firstByte &= SevenBitMask;

    std::uint32_t systemId = firstByte;
    for (std::size_t index = FirstCopiedAddressByte; index < MacAddressSize; ++index)
        systemId = (systemId << Formats::BitsPerByte) | macAddress[index];
    return systemId;
}

}
