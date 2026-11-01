#pragma once

#include "Formats/Bytes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Formats
{

// A message file (BMG file) holds the texts of a screen, one message after the other, in one language.
// The texts are UTF-16. The files of the System Menu have only the index (INF1) and the texts (DAT1); the files of the channels also
// give every message an id (MID1) and have escape sequences in their texts.
//
// An escape sequence stays in the text as the escape character followed by its bytes, two bytes to a character: first its size in
// bytes (the escape character included) and its group, then its kind, then its parameters. The program that shows the text replaces
// or drops it, for example a number to fill in or a change of color.
constexpr char16_t MessageEscapeCharacter = 0x001A;

struct MessageFile
{
    std::vector<std::u16string> messages;

    // The id of each message, in the order of the messages. Empty when the file has no ids.
    std::vector<std::uint32_t> messageIds;
};

MessageFile ReadMessageFile(ByteView bytes);

}
