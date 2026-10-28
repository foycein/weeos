#pragma once

#include <initializer_list>
#include <string>
#include <string_view>
#include <variant>

namespace Text
{

// A value for FormatText: a whole number or a text. A text is only read during the call, so it can be a view into a message.
using TextArgument = std::variant<int, std::u16string_view>;

// Puts arguments into the places of a format. The formats are written the way the formats of the menu are,
// which are formats of the C function swprintf:
//     %ls     a text
//     %d      a whole number
//     %02d    a whole number with at least two digits, filled up with zeros in front (any number of digits can stand instead of the 2,
//             the error codes of the menu are written with %06d)
//     %%      a percent sign
// Every place takes the next argument. A TextError is thrown when a place does not match its argument
// or when there are more places than arguments or the other way round.
std::u16string FormatText(std::u16string_view format, std::initializer_list<TextArgument> arguments);

// Some messages hold a placeholder that the menu replaces, for example "****" for a number of blocks.
// This replaces the first place where the placeholder stands. A TextError is thrown when the text has no such place.
std::u16string ReplacePlaceholder(std::u16string_view text, std::u16string_view placeholder, std::u16string_view replacement);

}
