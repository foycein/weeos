#include "Channels/OperaLanguageFile.h"

#include "Channels/ChannelError.h"

#include <charconv>
#include <format>

namespace Channels
{

namespace
{

constexpr std::string_view TranslationSection = "[Translation]";
constexpr std::uint32_t HashStart = 5381;
constexpr std::uint32_t HashFactor = 33;

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

std::string ResolveEscapes(std::string_view quotedText)
{
    std::string text;
    for (std::size_t index = 0; index < quotedText.size(); ++index)
    {
        char character = quotedText[index];
        if (character != '\\' || index + 1 == quotedText.size())
        {
            text += character;
            continue;
        }

        index += 1;
        char escaped = quotedText[index];
        if (escaped == 'n')
            text += '\n';
        else
            text += escaped;
    }
    return text;
}

}

OperaLanguageFile::OperaLanguageFile(Formats::ByteView bytes)
{
    std::string_view content(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    std::size_t sectionStart = content.find(TranslationSection);
    if (sectionStart == std::string_view::npos)
        throw ChannelError("The language file of the browser has no section [Translation]");

    std::string_view rest = content.substr(sectionStart + TranslationSection.size());
    while (!rest.empty())
    {
        std::size_t lineEnd = rest.find('\n');
        ReadLine(rest.substr(0, lineEnd));
        if (lineEnd == std::string_view::npos)
            break;

        rest = rest.substr(lineEnd + 1);
    }
}

// A line is id="text". Other lines, such as comments, are left alone.
void OperaLanguageFile::ReadLine(std::string_view line)
{
    std::size_t equalSign = line.find("=\"");
    std::size_t lastQuote = line.rfind('"');
    if (equalSign == std::string_view::npos || lastQuote <= equalSign + 1)
        return;

    std::int32_t id = 0;
    std::from_chars_result result = std::from_chars(line.data(), line.data() + equalSign, id);
    if (result.ec != std::errc())
        return;

    std::string_view quotedText = line.substr(equalSign + 2, lastQuote - equalSign - 2);
    texts[id] = Utf8ToText(ResolveEscapes(quotedText));
}

const std::u16string& OperaLanguageFile::Find(std::string_view name) const
{
    auto text = texts.find(IdOf(name));
    if (text == texts.end())
        throw ChannelError(std::format("The language file of the browser has no text {}", name));

    return text->second;
}

const std::u16string& OperaLanguageFile::FindById(std::int32_t id) const
{
    auto text = texts.find(id);
    if (text == texts.end())
        throw ChannelError(std::format("The language file of the browser has no text with the id {}", id));

    return text->second;
}

std::int32_t OperaLanguageFile::IdOf(std::string_view name)
{
    std::uint32_t hash = HashStart;
    for (char character : name)
        hash = hash * HashFactor + static_cast<unsigned char>(character);

    return static_cast<std::int32_t>(hash);
}

}
