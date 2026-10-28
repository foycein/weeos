#pragma once

#include "Formats/Bytes.h"

#include <string_view>

namespace Formats
{

// Turns a text like "0A447B97" into the four bytes 0x0A, 0x44, 0x7B, 0x97. Capital and small letters are both fine.
// Throws a FormatError when the text has an odd length or a character that is no hexadecimal digit.
Bytes ParseHexText(std::string_view hexText);

}
