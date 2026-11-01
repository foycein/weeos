#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace Settings
{

// A country that a console of the USA can be in: the two letters that Windows gives it (ISO 3166) and the code that the console keeps
// in its location setting (WiiBrew, page Country Codes).
struct Country
{
    std::string_view letters;
    std::uint8_t code;
};

// The country of a console that is not in the list of the USA console (docs/audit/OpenPoints/Settings.md SET-13).
constexpr std::uint8_t UnitedStatesCode = 49;

// The 48 countries that the Country page of a USA console offered, sorted by their letters.
std::span<const Country> ConsoleCountries();

// The console's code for the country of the PC, given by its two letters. United States for a country the USA console does not have.
std::uint8_t CountryCodeOfThePc(std::string_view letters);

}
