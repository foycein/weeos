#include "Text/TextFormat.h"

#include "Text/TextError.h"

#include <cstddef>

namespace Text
{

namespace
{

constexpr char16_t FormatMarker = u'%';
constexpr char16_t ZeroFlag = u'0';
constexpr char16_t SignedNumber = u'd';
constexpr char16_t UnsignedNumber = u'u';
constexpr std::u16string_view PercentSign = u"%";
constexpr std::u16string_view TextConversion = u"ls";
constexpr std::u16string_view LongNumberMark = u"ll";

// A number of digits above this is a mistake in the format.
constexpr int MostMinimumDigits = 99;

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

    // Only for numbers: the number is given as a std::int64_t, not as an int.
    bool isLong;

    // Only for numbers: the number must not be negative.
    bool isUnsigned;

    // The characters after the percent sign that belong to the conversion.
    std::size_t length;
};

bool IsDigit(char16_t character)
{
    return character >= u'0' && character <= u'9';
}

// Reads the zero and the digits after it, if there are any, and moves the length of the conversion behind them.
void ReadMinimumDigits(std::u16string_view afterMarker, Conversion& conversion)
{
    if (!afterMarker.starts_with(ZeroFlag))
        return;

    conversion.length = 1;
    while (conversion.length < afterMarker.size() && IsDigit(afterMarker[conversion.length]))
    {
        conversion.minimumDigits = conversion.minimumDigits * 10 + (afterMarker[conversion.length] - u'0');
        if (conversion.minimumDigits > MostMinimumDigits)
            throw TextError("a place for a number in the format asks for too many digits");

        ++conversion.length;
    }

    if (conversion.length == 1)
        throw TextError("a zero in the format is not followed by the number of digits");
}

Conversion ReadNumberConversion(std::u16string_view afterMarker)
{
    Conversion conversion = {ConversionKind::Number, 0, false, false, 0};
    ReadMinimumDigits(afterMarker, conversion);

    if (afterMarker.substr(conversion.length).starts_with(LongNumberMark))
    {
        conversion.isLong = true;
        conversion.length += LongNumberMark.size();
    }

    std::u16string_view afterOptions = afterMarker.substr(conversion.length);
    bool isSigned = afterOptions.starts_with(SignedNumber);
    bool isUnsigned = afterOptions.starts_with(UnsignedNumber);
    if (!isSigned && !isUnsigned)
        throw TextError("a percent sign in the format is not followed by ls, d, u, a zero and digits, ll or another percent sign");

    conversion.isUnsigned = isUnsigned;
    conversion.length += 1;
    return conversion;
}

Conversion ReadConversion(std::u16string_view afterMarker)
{
    if (afterMarker.starts_with(PercentSign))
        return {ConversionKind::Percent, 0, false, false, PercentSign.size()};

    if (afterMarker.starts_with(TextConversion))
        return {ConversionKind::Text, 0, false, false, TextConversion.size()};

    return ReadNumberConversion(afterMarker);
}

// Like the C formats, the zeros go behind the minus sign and the minus sign counts as a digit of the width.
void AppendNumber(std::u16string& text, std::int64_t number, int minimumDigits)
{
    std::string digits = std::to_string(number);
    std::size_t signLength = number < 0 ? 1 : 0;
    while (digits.size() < static_cast<std::size_t>(minimumDigits))
        digits.insert(signLength, "0");

    text.append(digits.begin(), digits.end());
}

std::int64_t ReadNumberArgument(const TextArgument& argument, const Conversion& conversion)
{
    if (conversion.isLong)
    {
        const std::int64_t* longNumber = std::get_if<std::int64_t>(&argument);
        if (longNumber == nullptr)
            throw TextError("a place for a number of 64 bits in the format got another kind of argument");

        return *longNumber;
    }

    const int* number = std::get_if<int>(&argument);
    if (number == nullptr)
        throw TextError("a place for a number in the format got another kind of argument");

    return *number;
}

void AppendArgument(std::u16string& text, const TextArgument& argument, const Conversion& conversion)
{
    if (conversion.kind == ConversionKind::Number)
    {
        std::int64_t number = ReadNumberArgument(argument, conversion);
        if (conversion.isUnsigned && number < 0)
            throw TextError("a place for a number that is not negative got a negative number");

        AppendNumber(text, number, conversion.minimumDigits);
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
    if (placeholder.empty())
        throw TextError("the placeholder is empty");

    std::size_t position = text.find(placeholder);
    if (position == std::u16string_view::npos)
        throw TextError("the text does not hold the placeholder");

    std::u16string replacedText;
    std::size_t copiedUpTo = 0;
    while (position != std::u16string_view::npos)
    {
        replacedText.append(text.substr(copiedUpTo, position - copiedUpTo));
        replacedText.append(replacement);
        copiedUpTo = position + placeholder.size();
        position = text.find(placeholder, copiedUpTo);
    }

    replacedText.append(text.substr(copiedUpTo));
    return replacedText;
}

}
