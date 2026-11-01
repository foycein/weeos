#pragma once

#include <string>

namespace SystemMenu
{

// A number as text, with leading zeros up to the minimum number of digits: 7 and 2 give "07".
inline std::u16string NumberText(int number, int minimumDigits = 1)
{
    constexpr int DecimalBase = 10;

    std::u16string text = std::u16string(1, u'0' + static_cast<char16_t>(number % DecimalBase));
    for (int rest = number / DecimalBase; rest > 0; rest /= DecimalBase)
        text.insert(text.begin(), u'0' + static_cast<char16_t>(rest % DecimalBase));

    while (static_cast<int>(text.size()) < minimumDigits)
        text.insert(text.begin(), u'0');

    return text;
}

}
