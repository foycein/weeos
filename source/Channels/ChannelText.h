#pragma once

#include <string>
#include <string_view>

namespace Channels
{

// The text boxes hold UTF-16 text. Digits, letters and signs of plain ASCII text keep their numbers.
inline std::u16string AsciiToText(std::string_view asciiText)
{
    std::u16string text;
    for (char character : asciiText)
        text += static_cast<char16_t>(character);

    return text;
}

}
