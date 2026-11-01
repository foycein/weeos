#pragma once

#include "Formats/Bytes.h"

#include <cstdint>
#include <map>
#include <string>

namespace Formats
{

// A message file (BMG file) whose messages carry numbers of their own (section MID1), as the Mii Channel's nigaoe_message_U.bmg, where
// "New Mii" is message 100100. The layouts name their texts by these numbers.
//
// A text may hold escape sequences (0x1A, a length, then data) where the program puts in a name or a number. They are left out:
// the texts are kept without them.
struct NumberedMessageFile
{
    std::map<std::uint32_t, std::u16string> messages;
};

// Throws a FormatError when the file is not a message file with numbered UTF-16 messages.
NumberedMessageFile ReadNumberedMessageFile(ByteView bytes);

}
