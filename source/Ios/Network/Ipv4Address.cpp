#include "Ios/Network/Ipv4Address.h"

#include <format>

namespace Ios
{

namespace
{

constexpr int PartCount = 4;
constexpr std::size_t MaximumDigitsPerPart = 3;
constexpr unsigned int LargestPart = 255;

bool IsDigit(char character)
{
    return character >= '0' && character <= '9';
}

// The part with this number. The first part is number 0.
unsigned int PartOf(Ipv4Address address, int partNumber)
{
    int shift = (PartCount - 1 - partNumber) * BitsPerIpv4Part;
    return (address >> shift) & LargestPart;
}

}

std::optional<Ipv4Address> ParseIpv4Address(std::string_view text)
{
    Ipv4Address address = 0;
    std::size_t position = 0;

    for (int partNumber = 0; partNumber < PartCount; ++partNumber)
    {
        std::size_t digitCount = 0;
        unsigned int part = 0;
        while (position < text.size() && IsDigit(text[position]) && digitCount <= MaximumDigitsPerPart)
        {
            part = part * 10 + static_cast<unsigned int>(text[position] - '0');
            ++position;
            ++digitCount;
        }

        if (digitCount == 0 || digitCount > MaximumDigitsPerPart || part > LargestPart)
            return std::nullopt;

        address = (address << BitsPerIpv4Part) | part;

        bool isLastPart = partNumber == PartCount - 1;
        if (isLastPart)
            break;

        if (position >= text.size() || text[position] != '.')
            return std::nullopt;

        ++position;
    }

    if (position != text.size())
        return std::nullopt;

    return address;
}

std::string FormatIpv4Address(Ipv4Address address)
{
    return std::format("{}.{}.{}.{}", PartOf(address, 0), PartOf(address, 1), PartOf(address, 2), PartOf(address, 3));
}

}
