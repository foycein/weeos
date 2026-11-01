#pragma once

#include "Formats/MessageFile.h"

#include <cstdint>
#include <map>
#include <string>
#include <string_view>

namespace Channels
{

// The id of a message of the Wii Speak Channel is the number of the layout it belongs to times 256 plus its number in that layout,
// for example 19010 * 256 + 2 for "Performance Check" of the settings menu (layout 19010_setMenu).
constexpr std::uint32_t WiiSpeakMessageId(std::uint32_t layoutNumber, std::uint32_t messageNumber)
{
    return layoutNumber * 256 + messageNumber;
}

// The messages of the Wii Speak Channel by their ids, with the escape sequences of the file worked out: a number to fill in is
// replaced by the text the program gives, and the changes of color and size are dropped (WeeOS shows the words in the color of the
// text box, a written deviation).
class WiiSpeakMessages
{
public:
    explicit WiiSpeakMessages(const Formats::MessageFile& file);

    // The text of the message; an empty text when the file has no message with this id. Every number to fill in becomes the argument.
    std::u16string Text(std::uint32_t messageId, std::u16string_view argument = {}) const;

private:
    std::map<std::uint32_t, std::u16string> messages;
};

}
