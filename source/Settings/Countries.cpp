#include "Settings/Countries.h"

#include <algorithm>
#include <array>

namespace Settings
{

namespace
{

// The Netherlands Antilles were split in 2010; their three parts keep the console's code of the Antilles.
constexpr std::array<Country, 51> Countries = {{
    {"AE", 168},
    {"AG", 9},
    {"AI", 8},
    {"AN", 38},
    {"AR", 10},
    {"AW", 11},
    {"BB", 13},
    {"BO", 15},
    {"BQ", 38},
    {"BR", 16},
    {"BS", 12},
    {"BZ", 14},
    {"CA", 18},
    {"CL", 20},
    {"CO", 21},
    {"CR", 22},
    {"CW", 38},
    {"DM", 23},
    {"DO", 24},
    {"EC", 25},
    {"GD", 28},
    {"GF", 27},
    {"GP", 29},
    {"GT", 30},
    {"GY", 31},
    {"HN", 33},
    {"HT", 32},
    {"JM", 34},
    {"KN", 43},
    {"KY", 19},
    {"LC", 44},
    {"MQ", 35},
    {"MS", 37},
    {"MX", 36},
    {"NI", 39},
    {"PA", 40},
    {"PE", 42},
    {"PY", 41},
    {"SA", 174},
    {"SG", 153},
    {"SR", 46},
    {"SV", 26},
    {"SX", 38},
    {"TC", 48},
    {"TT", 47},
    {"US", UnitedStatesCode},
    {"UY", 50},
    {"VC", 45},
    {"VE", 52},
    {"VG", 17},
    {"VI", 51},
}};

}

std::span<const Country> ConsoleCountries()
{
    return Countries;
}

std::uint8_t CountryCodeOfThePc(std::string_view letters)
{
    auto found = std::find_if(Countries.begin(), Countries.end(), [letters](const Country& country) { return country.letters == letters; });
    if (found == Countries.end())
        return UnitedStatesCode;

    return found->code;
}

}
