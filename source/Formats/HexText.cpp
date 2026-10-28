#include "Formats/HexText.h"

#include "Formats/FormatError.h"

#include <format>

namespace Formats
{

namespace
{

constexpr int HexDigitsPerByte = 2;
constexpr int BitsPerHexDigit = 4;
constexpr int FirstLetterValue = 10;

int HexDigitValue(char digit)
{
    if (digit >= '0' && digit <= '9')
        return digit - '0';

    if (digit >= 'a' && digit <= 'f')
        return digit - 'a' + FirstLetterValue;

    if (digit >= 'A' && digit <= 'F')
        return digit - 'A' + FirstLetterValue;

    throw FormatError(std::format("Hexadecimal text: '{}' is not a hexadecimal digit", digit));
}

}

Bytes ParseHexText(std::string_view hexText)
{
    if (hexText.size() % HexDigitsPerByte != 0)
        throw FormatError(std::format("Hexadecimal text: \"{}\" has an odd number of digits", hexText));

    Bytes bytes;
    for (std::size_t position = 0; position < hexText.size(); position += HexDigitsPerByte)
    {
        int highDigit = HexDigitValue(hexText[position]);
        int lowDigit = HexDigitValue(hexText[position + 1]);
        bytes.push_back(static_cast<Byte>((highDigit << BitsPerHexDigit) | lowDigit));
    }
    return bytes;
}

}
