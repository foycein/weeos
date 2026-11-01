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

// Text in UTF-8, as the pages of the browser and its language files hold it.
std::u16string Utf8ToText(std::string_view utf8);

}
