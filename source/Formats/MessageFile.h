#pragma once

#include "Formats/Bytes.h"

#include <string>
#include <vector>

namespace Formats
{

// A message file (BMG file) holds the texts of a screen, one message after the other, in one language.
// The texts are UTF-16. Only files without escape sequences and without message ids are supported,
// which is what all message files of the System Menu are.
struct MessageFile
{
    std::vector<std::u16string> messages;
};

MessageFile ReadMessageFile(ByteView bytes);

}
