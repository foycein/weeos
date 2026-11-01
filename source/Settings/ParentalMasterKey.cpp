#include "Settings/ParentalMasterKey.h"

#include <array>
#include <cstdint>
#include <format>

namespace Settings
{

namespace
{

constexpr std::uint64_t ConfirmationNumberLimit = 100000000;
constexpr std::uint32_t MasterKeyLimit = 100000;

// The key is the checksum of the date and the last four digits of the confirmation number, changed by two numbers of the menu.
constexpr std::size_t DigitsOfTheNumberThatCount = 4;
constexpr std::uint32_t KeyMixingPattern = 0xAAAA;
constexpr std::uint32_t KeyMixingOffset = 0x14C1;

// The checksum is CRC-32 with the reversed polynomial 0xEDB88320, started at all ones and not inverted at the end.
constexpr std::uint32_t ChecksumPolynomial = 0xEDB88320;
constexpr std::uint32_t ChecksumStart = 0xFFFFFFFF;
constexpr int BitsPerByte = 8;
constexpr std::uint32_t LowestByte = 0xFF;

std::uint32_t Checksum(const std::string& text)
{
    std::uint32_t checksum = ChecksumStart;
    for (char character : text)
    {
        checksum ^= static_cast<unsigned char>(character);
        for (int bit = 0; bit < BitsPerByte; ++bit)
        {
            bool lowestBitIsSet = (checksum & 1) != 0;
            checksum >>= 1;
            if (lowestBitIsSet)
                checksum ^= ChecksumPolynomial;
        }
    }

    return checksum;
}

std::u16string TextOfDigits(const std::string& digits)
{
    return std::u16string(digits.begin(), digits.end());
}

}

std::u16string ConfirmationNumber(const Ios::MacAddress& wirelessMacAddress)
{
    std::uint64_t addressAsNumber = 0;
    for (std::uint8_t addressByte : wirelessMacAddress)
        addressAsNumber = (addressAsNumber << BitsPerByte) | addressByte;

    return TextOfDigits(std::format("{:08}", addressAsNumber % ConfirmationNumberLimit));
}

std::u16string MasterKey(const std::u16string& confirmationNumber, int month, int day)
{
    std::string lastDigits;
    std::size_t firstDigitThatCounts = confirmationNumber.size() - DigitsOfTheNumberThatCount;
    for (std::size_t position = firstDigitThatCounts; position < confirmationNumber.size(); ++position)
        lastDigits.push_back(static_cast<char>(confirmationNumber[position] & LowestByte));

    std::string checkedText = std::format("{:02}{:02}{}", month, day, lastDigits);
    std::uint32_t key = ((Checksum(checkedText) ^ KeyMixingPattern) + KeyMixingOffset) % MasterKeyLimit;
    return TextOfDigits(std::format("{:05}", key));
}

}
