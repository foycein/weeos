#include "Text/TextFormat.h"

#include "Text/TextError.h"

#include <cstddef>

namespace Text
{

namespace
{

constexpr char16_t FormatMarker = u'%';
constexpr char16_t ZeroFlag = u'0';
constexpr char16_t NumberLetter = u'd';
constexpr std::u16string_view PercentConversion = u"%";
constexpr std::u16string_view TextConversion = u"ls";
constexpr std::size_t ZeroFlagLength = 1;
constexpr std::size_t NumberLetterLength = 1;
constexpr int DecimalBase = 10;

// A number of digits above this is a mistake in the format.
constexpr int MostMinimumDigits = 99;

constexpr const char* UnknownConversionMessage =
    "a percent sign in the format is not followed by ls, d, a zero and digits and d, or another percent sign";

enum class ConversionKind
{
    Percent,
    Text,
    Number,
};

// What stands after a percent sign.
struct Conversion
{
    ConversionKind kind;

    // Only for numbers: the zeros in front fill the number up to this many digits.
    int minimumDigits;

    // The characters after the percent sign that belong to the conversion.
    std::size_t length;
};

bool IsDigit(char16_t character)
{
    return character >= u'0' && character <= u'9';
}

// A zero, the number of digits and the letter d, for example 02d.
Conversion ReadNumberWithZeros(std::u16string_view afterMarker)
{
    if (!afterMarker.starts_with(ZeroFlag))
        throw TextError(UnknownConversionMessage);

    std::size_t length = ZeroFlagLength;
    int minimumDigits = 0;
    while (length < afterMarker.size() && IsDigit(afterMarker[length]))
    {
        int digit = afterMarker[length] - u'0';
        minimumDigits = minimumDigits * DecimalBase + digit;
        if (minimumDigits > MostMinimumDigits)
            throw TextError("a place for a number in the format asks for too many digits");

        ++length;
    }

    bool hasDigits = length > ZeroFlagLength;
    bool endsWithNumberLetter = length < afterMarker.size() && afterMarker[length] == NumberLetter;
    if (!hasDigits || !endsWithNumberLetter)
        throw TextError(UnknownConversionMessage);

    return {ConversionKind::Number, minimumDigits, length + NumberLetterLength};
}

Conversion ReadConversion(std::u16string_view afterMarker)
{
    if (afterMarker.starts_with(PercentConversion))
        return {ConversionKind::Percent, 0, PercentConversion.size()};

    if (afterMarker.starts_with(TextConversion))
        return {ConversionKind::Text, 0, TextConversion.size()};

    if (afterMarker.starts_with(NumberLetter))
        return {ConversionKind::Number, 0, NumberLetterLength};

    return ReadNumberWithZeros(afterMarker);
}

// Like the C formats, the zeros go behind the minus sign and the minus sign counts as a digit of the width.
void AppendNumber(std::u16string& text, int number, int minimumDigits)
{
    std::string digits = std::to_string(number);
    std::size_t signLength = number < 0 ? 1 : 0;
    while (digits.size() < static_cast<std::size_t>(minimumDigits))
        digits.insert(signLength, "0");

    text.append(digits.begin(), digits.end());
}

void AppendArgument(std::u16string& text, const TextArgument& argument, const Conversion& conversion)
{
    if (conversion.kind == ConversionKind::Number)
    {
        const int* number = std::get_if<int>(&argument);
        if (number == nullptr)
            throw TextError("a place for a number in the format got a text");

        AppendNumber(text, *number, conversion.minimumDigits);
        return;
    }

    const std::u16string_view* argumentText = std::get_if<std::u16string_view>(&argument);
    if (argumentText == nullptr)
        throw TextError("a place for a text in the format got a number");

    text.append(*argumentText);
}

}

std::u16string FormatText(std::u16string_view format, std::initializer_list<TextArgument> arguments)
{
    std::u16string text;
    const TextArgument* nextArgument = arguments.begin();
    std::size_t position = 0;

    while (position < format.size())
    {
        char16_t character = format[position];
        ++position;
        if (character != FormatMarker)
        {
            text.push_back(character);
            continue;
        }

        Conversion conversion = ReadConversion(format.substr(position));
        position += conversion.length;
        if (conversion.kind == ConversionKind::Percent)
        {
            text.push_back(FormatMarker);
            continue;
        }

        if (nextArgument == arguments.end())
            throw TextError("the format has more places than arguments");

        AppendArgument(text, *nextArgument, conversion);
        ++nextArgument;
    }

    if (nextArgument != arguments.end())
        throw TextError("there are more arguments than places in the format");

    return text;
}

std::u16string ReplacePlaceholder(std::u16string_view text, std::u16string_view placeholder, std::u16string_view replacement)
{
    std::size_t position = text.find(placeholder);
    if (position == std::u16string_view::npos)
        throw TextError("the text does not hold the placeholder");

    std::u16string replacedText(text);
    replacedText.replace(position, placeholder.size(), replacement);
    return replacedText;
}

}
