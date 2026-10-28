#include "Ios/Sockets/InternetAddress.h"

#include <cstdint>
#include <span>

namespace Ios
{

namespace
{

constexpr int MaximumNumberCount = 4;
constexpr std::uint32_t LargestNumberInFrontOfDot = 0xFF;
constexpr std::uint32_t LargestLastNumberOfTwo = 0xFFFFFF;
constexpr std::uint32_t LargestLastNumberOfThree = 0xFFFF;
constexpr std::uint32_t LowestByteMask = 0xFF;
constexpr unsigned char LargestAsciiCharacter = 0x7F;

constexpr int FirstPartShift = 3 * BitsPerIpv4Part;
constexpr int SecondPartShift = 2 * BitsPerIpv4Part;
constexpr int ThirdPartShift = BitsPerIpv4Part;

constexpr int DecimalBase = 10;
constexpr int OctalBase = 8;
constexpr int HexadecimalBase = 16;

bool IsDigit(char character)
{
    return character >= '0' && character <= '9';
}

bool IsLowerHexLetter(char character)
{
    return character >= 'a' && character <= 'f';
}

bool IsUpperHexLetter(char character)
{
    return character >= 'A' && character <= 'F';
}

bool IsLetterOrDigit(char character)
{
    bool isLetter = (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z');
    return isLetter || IsDigit(character);
}

// The text with a place in it. Behind the end of the text the character is zero, like the end of a C string.
class AddressText
{
public:
    explicit AddressText(std::string_view textToRead)
        : text(textToRead)
    {
    }

    char Current() const
    {
        return position < text.size() ? text[position] : '\0';
    }

    void MoveOn()
    {
        ++position;
    }

    // Reads the number that starts at the place and moves behind it. Nothing when no number starts there.
    // A number that does not fit 32 bits loses its upper bits.
    std::optional<std::uint32_t> ReadNumber()
    {
        if (!IsDigit(Current()))
            return std::nullopt;

        int base = ReadBasePrefix();
        std::uint32_t number = 0;
        while (true)
        {
            char character = Current();
            if (IsDigit(character))
                number = number * base + (character - '0');
            else if (base == HexadecimalBase && IsLowerHexLetter(character))
                number = number * base + (character - 'a' + DecimalBase);
            else if (base == HexadecimalBase && IsUpperHexLetter(character))
                number = number * base + (character - 'A' + DecimalBase);
            else
                return number;

            MoveOn();
        }
    }

private:
    // A number that starts with 0 is octal, with 0x it is hexadecimal. The prefix is skipped, a lone 0 stays.
    int ReadBasePrefix()
    {
        if (Current() != '0')
            return DecimalBase;

        MoveOn();
        if (Current() != 'x' && Current() != 'X')
            return OctalBase;

        MoveOn();
        return HexadecimalBase;
    }

    std::string_view text;
    std::size_t position = 0;
};

// The numbers of the text as the four bytes of the address. Nothing when the last number is too big for its place.
std::optional<Ipv4Address> AssembleAddress(std::span<const std::uint32_t> numbers)
{
    std::uint32_t last = numbers.back();

    if (numbers.size() == 1)
        return last;

    if (numbers.size() == 2)
    {
        if (last > LargestLastNumberOfTwo)
            return std::nullopt;

        return (numbers[0] << FirstPartShift) | last;
    }

    if (numbers.size() == 3)
    {
        if (last > LargestLastNumberOfThree)
            return std::nullopt;

        return (numbers[0] << FirstPartShift) | (numbers[1] << SecondPartShift) | last;
    }

    return (numbers[0] << FirstPartShift) | (numbers[1] << SecondPartShift) | (numbers[2] << ThirdPartShift) | (last & LowestByteMask);
}

}

std::optional<Ipv4Address> ParseInternetAddress(std::string_view text)
{
    AddressText addressText(text);
    std::uint32_t numbers[MaximumNumberCount];
    int numberCount = 0;

    while (true)
    {
        std::optional<std::uint32_t> number = addressText.ReadNumber();
        if (!number)
            return std::nullopt;

        numbers[numberCount] = *number;
        ++numberCount;

        if (addressText.Current() != '.')
            break;

        bool isAnotherNumberTooMany = numberCount == MaximumNumberCount;
        if (isAnotherNumberTooMany || *number > LargestNumberInFrontOfDot)
            return std::nullopt;

        addressText.MoveOn();
    }

    char following = addressText.Current();
    bool isOutsideAscii = static_cast<unsigned char>(following) > LargestAsciiCharacter;
    if (isOutsideAscii || IsLetterOrDigit(following))
        return std::nullopt;

    return AssembleAddress(std::span<const std::uint32_t>(numbers, numberCount));
}

}
