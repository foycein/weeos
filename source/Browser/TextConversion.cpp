#include "Browser/TextConversion.h"

#include <windows.h>

namespace Browser
{

std::wstring WideTextOf(std::string_view utf8Text)
{
    if (utf8Text.empty())
        return {};

    int utf8Length = static_cast<int>(utf8Text.size());
    int wideLength = MultiByteToWideChar(CP_UTF8, 0, utf8Text.data(), utf8Length, nullptr, 0);
    std::wstring wideText(static_cast<std::size_t>(wideLength), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8Text.data(), utf8Length, wideText.data(), wideLength);
    return wideText;
}

std::wstring WideTextOf(std::u16string_view text)
{
    return std::wstring(text.begin(), text.end());
}

std::string Utf8TextOf(std::wstring_view wideText)
{
    if (wideText.empty())
        return {};

    int wideLength = static_cast<int>(wideText.size());
    int utf8Length = WideCharToMultiByte(CP_UTF8, 0, wideText.data(), wideLength, nullptr, 0, nullptr, nullptr);
    std::string utf8Text(static_cast<std::size_t>(utf8Length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wideText.data(), wideLength, utf8Text.data(), utf8Length, nullptr, nullptr);
    return utf8Text;
}

std::u16string TextOf(std::wstring_view wideText)
{
    return std::u16string(wideText.begin(), wideText.end());
}

std::string Utf8TextOf(std::u16string_view text)
{
    return Utf8TextOf(WideTextOf(text));
}

std::u16string TextOfUtf8(std::string_view utf8Text)
{
    return TextOf(WideTextOf(utf8Text));
}

}
