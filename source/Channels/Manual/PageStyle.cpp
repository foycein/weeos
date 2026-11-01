#include "Channels/Manual/PageLayouter.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>

namespace Channels
{

namespace
{

// A point of a font size is 4/3 of a pixel on the screen of the browser.
constexpr float PixelsPerPoint = 4.0f / 3.0f;

// <font size="1"> to <font size="7"> in pixels, and the size that relative sizes ("-4") count from.
constexpr std::array<float, 7> FontSizeSteps = {10, 13, 16, 18, 24, 32, 48};
constexpr int NormalFontSizeStep = 3;

// A weight from 600 on is bold, as "bold" and "bolder" are.
constexpr int FirstBoldWeight = 600;

constexpr int HexadecimalBase = 16;
constexpr std::size_t ShortColorDigits = 3;
constexpr std::size_t LongColorDigits = 6;
constexpr int BitsPerByte = 8;
constexpr int BitsPerHexadecimalDigit = 4;
constexpr unsigned int HexadecimalDigitMask = 0xF;
constexpr unsigned int ByteMask = 0xFF;

// A short color repeats each digit: #3a0 is #33aa00, so a digit counts 17 times.
constexpr int ShortDigitRepeat = 17;

// Wide enough for any line of a page, so that a text is measured on one line.
constexpr float UnlimitedTextWidth = 100000;
constexpr std::uint8_t Opaque = 255;

// The colors the pages name by a word.
struct NamedColor
{
    std::string_view name;
    Formats::Rgba8Color color;
};

constexpr std::array<NamedColor, 6> NamedColors = {{
    {"white", {255, 255, 255, Opaque}},
    {"black", {0, 0, 0, Opaque}},
    {"red", {255, 0, 0, Opaque}},
    {"gray", {128, 128, 128, Opaque}},
    {"grey", {128, 128, 128, Opaque}},
    {"blue", {0, 0, 255, Opaque}},
}};

constexpr std::array<std::string_view, 15> BlockElements = {
    "html", "body", "div", "p", "table", "center", "form", "ul", "ol", "li", "h1", "h2", "h3", "frameset", "hr"};

constexpr std::array<std::string_view, 9> HiddenElements = {"head", "title", "meta", "link", "script", "style", "noframes", "frame", "input"};

// The browser keeps a margin around the body unless the page sets it (Opera's 8 pixels).
constexpr float BodyMargin = 8;

std::string ToLowerCase(std::string_view text)
{
    std::string lower(text);
    for (char& character : lower)
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));

    return lower;
}

std::vector<std::string_view> Words(std::string_view text)
{
    std::vector<std::string_view> words;
    std::size_t position = 0;
    while (position < text.size())
    {
        while (position < text.size() && std::isspace(static_cast<unsigned char>(text[position])))
            ++position;

        std::size_t end = position;
        while (end < text.size() && !std::isspace(static_cast<unsigned char>(text[end])))
            ++end;

        if (end > position)
            words.push_back(text.substr(position, end - position));
        position = end;
    }
    return words;
}

std::optional<float> ReadNumber(std::string_view text)
{
    float number = 0;
    auto result = std::from_chars(text.data(), text.data() + text.size(), number);
    if (result.ec != std::errc())
        return std::nullopt;

    return number;
}

TextAlignment ReadAlignment(std::string_view text, TextAlignment otherwise)
{
    std::string word = ToLowerCase(text);
    if (word == "center" || word == "middle")
        return TextAlignment::Center;
    if (word == "right")
        return TextAlignment::Right;
    if (word == "left")
        return TextAlignment::Left;

    return otherwise;
}

float FontSizeOfStep(int step)
{
    int index = std::clamp(step, 1, static_cast<int>(FontSizeSteps.size())) - 1;
    return FontSizeSteps[static_cast<std::size_t>(index)];
}

// <font size="-4"> counts from the normal size, <font size="2"> is a step of its own.
float FontSizeOfAttribute(std::string_view text, float otherwise)
{
    bool isRelative = text.starts_with('+') || text.starts_with('-');
    std::string_view digits = text.starts_with('+') ? text.substr(1) : text;
    std::optional<float> number = ReadNumber(digits);
    if (!number)
        return otherwise;

    int step = static_cast<int>(*number);
    return FontSizeOfStep(isRelative ? NormalFontSizeStep + step : step);
}

// "url(img/a.png)" or url("img/a.png") gives img/a.png.
std::string UrlOf(std::string_view value)
{
    std::size_t open = value.find('(');
    std::size_t close = value.rfind(')');
    if (open == std::string_view::npos || close == std::string_view::npos || close < open)
        return {};

    std::string_view inside = value.substr(open + 1, close - open - 1);
    while (!inside.empty() && (inside.front() == '"' || inside.front() == '\'' || inside.front() == ' '))
        inside.remove_prefix(1);
    while (!inside.empty() && (inside.back() == '"' || inside.back() == '\'' || inside.back() == ' '))
        inside.remove_suffix(1);

    return std::string(inside);
}

// "18px 1px 0 10px" sets top, right, bottom and left; fewer values repeat as CSS says.
void ReadBoxSides(std::string_view value, float fontSize, float& top, float& right, float& bottom, float& left)
{
    std::vector<float> sides;
    for (std::string_view word : Words(value))
    {
        std::optional<PageLength> length = ReadLength(word, fontSize);
        sides.push_back(length ? length->value : 0);
    }

    if (sides.empty())
        return;

    top = sides[0];
    right = sides.size() > 1 ? sides[1] : top;
    bottom = sides.size() > 2 ? sides[2] : top;
    left = sides.size() > 3 ? sides[3] : right;
}

}

float PageLength::Of(float room) const
{
    if (isPercentage)
        return room * value / 100;

    return value;
}

BoxStyle BoxStyle::InheritedPart() const
{
    BoxStyle inherited;
    inherited.look = look;
    inherited.textAlignment = textAlignment;
    inherited.textIndent = textIndent;
    return inherited;
}

std::optional<PageLength> ReadLength(std::string_view text, float fontSize)
{
    std::string word = ToLowerCase(text);
    std::optional<float> number = ReadNumber(word);
    if (!number)
        return std::nullopt;

    if (word.ends_with("%"))
        return PageLength{*number, true};
    if (word.ends_with("pt"))
        return PageLength{*number * PixelsPerPoint, false};
    if (word.ends_with("em"))
        return PageLength{*number * fontSize, false};

    return PageLength{*number, false};
}

std::optional<Formats::Rgba8Color> ReadColor(std::string_view text)
{
    std::string word = ToLowerCase(text);
    for (const NamedColor& named : NamedColors)
    {
        if (named.name == word)
            return named.color;
    }

    if (!word.starts_with('#'))
        return std::nullopt;

    std::string_view digits = std::string_view(word).substr(1);
    unsigned int number = 0;
    auto result = std::from_chars(digits.data(), digits.data() + digits.size(), number, HexadecimalBase);
    if (result.ec != std::errc())
        return std::nullopt;

    if (digits.size() == ShortColorDigits)
    {
        auto digit = [&](int position) {
            unsigned int value = (number >> (position * BitsPerHexadecimalDigit)) & HexadecimalDigitMask;
            return static_cast<std::uint8_t>(value * ShortDigitRepeat);
        };
        return Formats::Rgba8Color{digit(2), digit(1), digit(0), Opaque};
    }

    if (digits.size() != LongColorDigits)
        return std::nullopt;

    auto byte = [&](int position) { return static_cast<std::uint8_t>((number >> (position * BitsPerByte)) & ByteMask); };
    return Formats::Rgba8Color{byte(2), byte(1), byte(0), Opaque};
}

bool IsBlockElement(std::string_view tag)
{
    return std::find(BlockElements.begin(), BlockElements.end(), tag) != BlockElements.end();
}

bool IsHiddenElement(std::string_view tag)
{
    return std::find(HiddenElements.begin(), HiddenElements.end(), tag) != HiddenElements.end();
}

BoxStyle PageLayouter::StyleOf(const HtmlNode& element, const BoxStyle& parentStyle) const
{
    BoxStyle style = parentStyle.InheritedPart();
    const std::string& tag = element.tag;

    if (tag == "b" || tag == "strong" || tag == "th")
        style.look.isBold = true;
    if (tag == "th" || tag == "center")
        style.textAlignment = TextAlignment::Center;
    if (tag == "p")
    {
        style.marginTop = style.look.size;
        style.marginBottom = style.look.size;
    }
    if (tag == "body")
    {
        style.paddingTop = element.HasAttribute("topmargin") ? 0 : BodyMargin;
        style.paddingLeft = element.HasAttribute("leftmargin") ? 0 : BodyMargin;
        style.paddingRight = style.paddingLeft;
    }

    ApplyAttributes(element, style);
    for (const StyleDeclaration& declaration : styleSheet.DeclarationsFor(element, folder))
        ApplyDeclaration(declaration, style);

    return style;
}

void PageLayouter::ApplyAttributes(const HtmlNode& element, BoxStyle& style) const
{
    const std::string& tag = element.tag;
    bool takesAlignment = tag == "div" || tag == "p" || tag == "td" || tag == "th" || tag == "tr";
    if (takesAlignment && element.HasAttribute("align"))
        style.textAlignment = ReadAlignment(element.Attribute("align"), style.textAlignment);

    if (element.HasAttribute("bgcolor"))
        style.backgroundColor = ReadColor(element.Attribute("bgcolor"));

    bool takesBackground = tag == "td" || tag == "th" || tag == "table" || tag == "body" || tag == "tr";
    if (takesBackground && element.HasAttribute("background"))
        style.backgroundPath = ResolvePath(path, element.Attribute("background"));

    if (tag == "font" && element.HasAttribute("size"))
        style.look.size = FontSizeOfAttribute(element.Attribute("size"), style.look.size);
    if (tag == "font" && element.HasAttribute("color"))
        style.look.color = ReadColor(element.Attribute("color")).value_or(style.look.color);
}

void PageLayouter::ApplyDeclaration(const StyleDeclaration& declaration, BoxStyle& style) const
{
    const std::string& property = declaration.property;
    std::string_view value = declaration.value;
    float fontSize = style.look.size;

    if (property == "font-size")
        style.look.size = ReadLength(value, fontSize).value_or(PageLength{fontSize, false}).value;
    else if (property == "color")
        style.look.color = ReadColor(value).value_or(style.look.color);
    else if (property == "font-weight")
    {
        std::string weight = ToLowerCase(value);
        std::optional<float> number = ReadNumber(weight);
        style.look.isBold = weight == "bold" || weight == "bolder" || (number && *number >= FirstBoldWeight);
    }
    else if (property == "letter-spacing")
        style.look.letterSpacing = ReadLength(value, fontSize).value_or(PageLength{}).value;
    else if (property == "text-align")
        style.textAlignment = ReadAlignment(value, style.textAlignment);
    else if (property == "text-indent")
        style.textIndent = ReadLength(value, fontSize).value_or(PageLength{}).value;
    else if (property == "padding")
        ReadBoxSides(value, fontSize, style.paddingTop, style.paddingRight, style.paddingBottom, style.paddingLeft);
    else if (property == "padding-top")
        style.paddingTop = ReadLength(value, fontSize).value_or(PageLength{}).value;
    else if (property == "padding-right")
        style.paddingRight = ReadLength(value, fontSize).value_or(PageLength{}).value;
    else if (property == "padding-bottom")
        style.paddingBottom = ReadLength(value, fontSize).value_or(PageLength{}).value;
    else if (property == "padding-left")
        style.paddingLeft = ReadLength(value, fontSize).value_or(PageLength{}).value;
    else if (property == "margin")
    {
        float right = 0;
        float left = 0;
        ReadBoxSides(value, fontSize, style.marginTop, right, style.marginBottom, left);
    }
    else if (property == "margin-top")
        style.marginTop = ReadLength(value, fontSize).value_or(PageLength{}).value;
    else if (property == "margin-bottom")
        style.marginBottom = ReadLength(value, fontSize).value_or(PageLength{}).value;
    else if (property == "width")
        style.width = ReadLength(value, fontSize);
    else if (property == "height")
        style.height = ReadLength(value, fontSize);
    else if (property == "background-image")
        style.backgroundPath = ResolvePath(declaration.baseFolder, UrlOf(value));
    else if (property == "background-color")
        style.backgroundColor = ReadColor(value);
}

float PageLayouter::TextLineHeight(const PageTextLook& look) const
{
    Text::TextStyle textStyle;
    textStyle.fontSize = {look.size, look.size};
    Text::TextArea area;
    area.width = UnlimitedTextWidth;
    area.height = UnlimitedTextWidth;
    Text::LaidOutText laidOut = font.LayOut(u"X", textStyle, area);
    if (laidOut.lines.empty())
        return look.size;

    return laidOut.lines.front().height;
}

float PageLayouter::TextWidth(std::u16string_view text, const PageTextLook& look) const
{
    if (text.empty())
        return 0;

    Text::TextStyle textStyle;
    textStyle.fontSize = {look.size, look.size};
    textStyle.characterSpace = look.letterSpacing;
    Text::TextArea area;
    area.width = UnlimitedTextWidth;
    area.height = UnlimitedTextWidth;
    Text::LaidOutText laidOut = font.LayOut(text, textStyle, area);
    if (laidOut.lines.empty())
        return 0;

    return laidOut.lines.front().width;
}

}
