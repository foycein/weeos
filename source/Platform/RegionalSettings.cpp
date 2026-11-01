#include "Platform/RegionalSettings.h"

#include <Windows.h>

#include <string_view>

namespace Platform
{

namespace
{

// "HH:mm:ss" or "h:mm:ss tt": an H stands for the hours of 24, an h for the hours of 12, and two of them for a leading 0.
constexpr wchar_t TwentyFourHourLetter = L'H';
constexpr std::wstring_view TwoDigitHoursOfTwentyFour = L"HH";
constexpr std::wstring_view TwoDigitHoursOfTwelve = L"hh";

constexpr int LongestGeographicName = 16;
constexpr int LongestTimeFormat = 80;

std::string ReadCountryLetters()
{
    wchar_t name[LongestGeographicName] = {};
    int length = GetUserDefaultGeoName(name, LongestGeographicName);
    if (length <= 1)
        return {};

    // A country has two letters. A larger region has three digits, which match no country of the console.
    std::string letters;
    for (int index = 0; index < length - 1; ++index)
        letters.push_back(static_cast<char>(name[index]));

    return letters;
}

std::wstring ReadTimeFormat()
{
    wchar_t format[LongestTimeFormat] = {};
    int length = GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_STIMEFORMAT, format, LongestTimeFormat);
    if (length <= 1)
        return {};

    return std::wstring(format, length - 1);
}

}

RegionalSettings ReadRegionalSettings()
{
    RegionalSettings settings;

    std::string letters = ReadCountryLetters();
    if (!letters.empty())
        settings.countryLetters = letters;

    std::wstring timeFormat = ReadTimeFormat();
    if (timeFormat.empty())
        return settings;

    settings.usesTwentyFourHourClock = timeFormat.find(TwentyFourHourLetter) != std::wstring::npos;
    bool hasTwoDigitHours = timeFormat.find(TwoDigitHoursOfTwentyFour) != std::wstring::npos
        || timeFormat.find(TwoDigitHoursOfTwelve) != std::wstring::npos;
    settings.showsLeadingZeroOfHours = hasTwoDigitHours;
    return settings;
}

}
