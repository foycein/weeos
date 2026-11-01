#pragma once

#include <string>

namespace SystemMenu
{

// A number as text, with leading zeros up to the minimum number of digits: 7 and 2 give "07".
inline std::u16string NumberText(int number, int minimumDigits = 1)
{
    std::u16string text = std::u16string(1, u'0' + static_cast<char16_t>(number % 10));
    for (int rest = number / 10; rest > 0; rest /= 10)
        text.insert(text.begin(), u'0' + static_cast<char16_t>(rest % 10));

    while (static_cast<int>(text.size()) < minimumDigits)
        text.insert(text.begin(), u'0');

    return text;
}

}
