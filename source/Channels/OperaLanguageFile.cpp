#include "Channels/OperaLanguageFile.h"

#include "Channels/ChannelError.h"
#include "Channels/ChannelText.h"

#include <charconv>
#include <format>

namespace Channels
{

namespace
{

constexpr std::string_view TranslationSection = "[Translation]";
constexpr std::uint32_t HashStart = 5381;
constexpr std::uint32_t HashFactor = 33;

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
