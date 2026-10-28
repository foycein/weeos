#pragma once

#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <variant>

namespace Text
{

// A value for FormatText: a whole number, a whole number of 64 bits (the friend code of a Wii) or a text.
// A text is only read during the call, so it can be a view into a message.
using TextArgument = std::variant<int, std::int64_t, std::u16string_view>;

// Puts arguments into the places of a format. The formats are written the way the formats of the menu are,
// which are formats of the C function swprintf:
//     %ls       a text
//     %d        a whole number
//     %02d      a whole number with at least two digits, filled up with zeros in front (any number of digits can stand instead of the 2)
//     %lld      a whole number of 64 bits, which is given as a std::int64_t
//     %016llu   a whole number of 64 bits that is not negative, with at least 16 digits
//     %u        a whole number that is not negative
//     %%        a percent sign
// Every place takes the next argument. A TextError is thrown when a place does not match its argument
// or when there are more places than arguments or the other way round.
std::u16string FormatText(std::u16string_view format, std::initializer_list<TextArgument> arguments);

// Some messages hold a placeholder that the menu replaces, for example "xxxxxxxxxx" for a name, "****" for a number of blocks and
// "1234567890123456" for a friend code. This replaces every place where the placeholder stands: the mail to a friend holds the
// friend code four times, and the menu replaces all of them (0x813871b0). A TextError is thrown when the text has no such place.
std::u16string ReplacePlaceholder(std::u16string_view text, std::u16string_view placeholder, std::u16string_view replacement);

}
