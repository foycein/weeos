#pragma once

namespace Ios
{

// The number of an IOS: IOS80 is the title 00000001-00000050.
using IosNumber = int;

// A run of IOS numbers, for example 12 to 15. A single number is a run of one.
struct IosNumberRange
{
    IosNumber first;
    IosNumber last;

    IosNumberRange(IosNumber number)
        : first(number)
        , last(number)
    {
    }

    IosNumberRange(IosNumber firstNumber, IosNumber lastNumber)
        : first(firstNumber)
        , last(lastNumber)
    {
    }

    bool Contains(IosNumber number) const
    {
        return number >= first && number <= last;
    }
};

// Lets a table say "33 through 38" for the IOS 33, 34, 35, 36, 37 and 38.
inline IosNumberRange Through(IosNumber first, IosNumber last)
{
    return IosNumberRange(first, last);
}

}
