#include "Channels/WiiSpeak/WiiSpeakMessages.h"

namespace Channels
{

namespace
{

constexpr int EscapeSizeShift = 8;
constexpr char16_t EscapeGroupMask = 0x00FF;

// The group of the escapes that stand for something the program fills in (a name, a number, a date); the others change the look.
constexpr char16_t FillInGroup = 0x01;

}

WiiSpeakMessages::WiiSpeakMessages(const Formats::MessageFile& file)
{
    for (std::size_t index = 0; index < file.messages.size() && index < file.messageIds.size(); ++index)
        messages[file.messageIds[index]] = file.messages[index];
}

std::u16string WiiSpeakMessages::Text(std::uint32_t messageId, std::u16string_view argument) const
{
    auto found = messages.find(messageId);
    if (found == messages.end())
        return {};

    const std::u16string& stored = found->second;
    std::u16string text;
    std::size_t position = 0;
    while (position < stored.size())
    {
        char16_t character = stored[position];
        if (character != Formats::MessageEscapeCharacter)
        {
            text.push_back(character);
            ++position;
            continue;
        }

        // The reader of the file has checked that every escape is complete and at least two characters long.
        char16_t sizeAndGroup = stored[position + 1];
        std::size_t escapeBytes = sizeAndGroup >> EscapeSizeShift;
        std::size_t escapeCharacters = escapeBytes / sizeof(char16_t);
        bool isFillIn = (sizeAndGroup & EscapeGroupMask) == FillInGroup;
        if (isFillIn)
            text += argument;

        position += escapeCharacters;
    }
    return text;
}

}
