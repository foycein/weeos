#pragma once

#include <string>
#include <string_view>

namespace Browser
{

// WebView2 speaks the wide text of Windows (UTF-16, like the texts of WeeOS); addresses are kept as UTF-8.
std::wstring WideTextOf(std::string_view utf8Text);
std::wstring WideTextOf(std::u16string_view text);
std::string Utf8TextOf(std::wstring_view wideText);
std::u16string TextOf(std::wstring_view wideText);

}
