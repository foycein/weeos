#include "Formats/SettingTextFile.h"

#include "Formats/FormatError.h"

#include <bit>
#include <format>

namespace Formats
{

namespace
{

constexpr std::size_t FileSize = 256;
constexpr std::uint32_t FirstKey = 0x73B5DBFA;

// Every byte is combined with the lowest byte of a key. After each byte the key is turned left by one bit.
std::string DecryptText(ByteView encryptedBytes)
{
    std::string text;
    std::uint32_t key = FirstKey;

    for (Byte encryptedByte : encryptedBytes)
    {
        text.push_back(static_cast<char>(encryptedByte ^ (key & ByteMask)));
        key = std::rotl(key, 1);
    }
    return text;
}

Bytes EncryptText(const std::string& text)
{
    Bytes encryptedBytes;
    std::uint32_t key = FirstKey;

    for (char character : text)
    {
        encryptedBytes.push_back(static_cast<Byte>(static_cast<Byte>(character) ^ (key & ByteMask)));
        key = std::rotl(key, 1);
    }
    return encryptedBytes;
}

// The console fills the file up with zero bytes that it never encrypted. They would decrypt to junk behind the text,
// so they are cut off first. A file that was encrypted up to its end has no zeros there, and nothing is cut.
ByteView WithoutTrailingZeros(ByteView bytes)
{
    while (!bytes.empty() && bytes.back() == 0)
        bytes = bytes.first(bytes.size() - 1);

    return bytes;
}

// The text ends at the first zero. The lines end with a carriage return and a line feed.
std::vector<std::string> SplitIntoLines(const std::string& text)
{
    std::string usedText = text.substr(0, text.find('\0'));

    std::vector<std::string> lines;
    std::size_t lineStart = 0;
    while (lineStart < usedText.size())
    {
        std::size_t lineEnd = usedText.find('\n', lineStart);
        if (lineEnd == std::string::npos)
            lineEnd = usedText.size();

        std::string line = usedText.substr(lineStart, lineEnd - lineStart);
        if (line.ends_with('\r'))
            line.pop_back();

        if (!line.empty())
            lines.push_back(line);

        lineStart = lineEnd + 1;
    }
    return lines;
}

SettingTextEntry ParseLine(const std::string& line)
{
    std::size_t equalsPosition = line.find('=');
    if (equalsPosition == std::string::npos)
        throw FormatError(std::format("Setting text file: the line \"{}\" has no equals sign", line));

    return {line.substr(0, equalsPosition), line.substr(equalsPosition + 1)};
}

}

SettingTextFile ReadSettingTextFile(ByteView encryptedBytes)
{
    if (encryptedBytes.size() != FileSize)
    {
        throw FormatError(std::format("Setting text file: the file has {} bytes instead of {}", encryptedBytes.size(), FileSize));
    }

    SettingTextFile file;
    std::string text = DecryptText(WithoutTrailingZeros(encryptedBytes));
    for (const std::string& line : SplitIntoLines(text))
        file.entries.push_back(ParseLine(line));

    return file;
}

Bytes WriteSettingTextFile(const SettingTextFile& file)
{
    std::string text;
    for (const SettingTextEntry& entry : file.entries)
        text += entry.name + "=" + entry.value + "\r\n";

    if (text.size() >= FileSize)
        throw FormatError(std::format("Setting text file: the text has {} characters, more than the file holds", text.size()));

    Bytes bytes = EncryptText(text);
    bytes.resize(FileSize, 0);
    return bytes;
}

std::optional<std::string> SettingTextFile::FindValue(std::string_view name) const
{
    for (const SettingTextEntry& entry : entries)
    {
        if (entry.name == name)
            return entry.value;
    }

    return std::nullopt;
}

}
