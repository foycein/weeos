#include "Channels/ChannelText.h"

namespace Channels
{

namespace
{

// The bits that a byte of UTF-8 adds to its character.
constexpr unsigned char ContinuationBits = 0x3F;
constexpr unsigned char TwoByteStartBits = 0x1F;
constexpr unsigned char ThreeByteStartBits = 0x0F;
constexpr unsigned char FourByteStartBits = 0x07;
constexpr unsigned char FirstOfTwoBytes = 0xC0;
constexpr unsigned char FirstOfThreeBytes = 0xE0;
constexpr unsigned char FirstOfFourBytes = 0xF0;
constexpr char32_t FirstCharacterBeyondSixteenBits = 0x10000;
constexpr char32_t HighSurrogateStart = 0xD800;
constexpr char32_t LowSurrogateStart = 0xDC00;
constexpr int SurrogateBits = 10;
constexpr char32_t LowSurrogateMask = 0x3FF;

std::size_t LengthOfCharacterAt(unsigned char firstByte)
{
    if (firstByte >= FirstOfFourBytes)
        return 4;
    if (firstByte >= FirstOfThreeBytes)
        return 3;
    if (firstByte >= FirstOfTwoBytes)
        return 2;
    return 1;
}

char32_t StartBitsOf(unsigned char firstByte, std::size_t length)
{
    if (length == 4)
        return firstByte & FourByteStartBits;
    if (length == 3)
        return firstByte & ThreeByteStartBits;
    if (length == 2)
        return firstByte & TwoByteStartBits;
    return firstByte;
}

void AppendCharacter(std::u16string& text, char32_t character)
{
    if (character < FirstCharacterBeyondSixteenBits)
    {
        text += static_cast<char16_t>(character);
        return;
    }

    char32_t beyond = character - FirstCharacterBeyondSixteenBits;
    text += static_cast<char16_t>(HighSurrogateStart + (beyond >> SurrogateBits));
    text += static_cast<char16_t>(LowSurrogateStart + (beyond & LowSurrogateMask));
}

}

std::u16string Utf8ToText(std::string_view utf8)
{
    std::u16string text;
    std::size_t index = 0;
    while (index < utf8.size())
    {
        unsigned char firstByte = static_cast<unsigned char>(utf8[index]);
        std::size_t length = LengthOfCharacterAt(firstByte);
        char32_t character = StartBitsOf(firstByte, length);
        for (std::size_t next = 1; next < length && index + next < utf8.size(); ++next)
            character = (character << 6) | (static_cast<unsigned char>(utf8[index + next]) & ContinuationBits);

        AppendCharacter(text, character);
        index += length;
    }
    return text;
}

}
