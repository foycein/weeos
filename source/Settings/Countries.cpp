#include "Settings/Countries.h"

#include <algorithm>
#include <array>

namespace Settings
{

namespace
{

constexpr std::array<Country, 48> Countries = {{
    {u"Anguilla", 8},
    {u"Antigua and Barbuda", 9},
    {u"Argentina", 10},
    {u"Aruba", 11},
    {u"Bahamas", 12},
    {u"Barbados", 13},
    {u"Belize", 14},
    {u"Bolivia", 15},
    {u"Brazil", 16},
    {u"British Virgin Islands", 17},
    {u"Canada", 18},
    {u"Cayman Islands", 19},
    {u"Chile", 20},
    {u"Colombia", 21},
    {u"Costa Rica", 22},
    {u"Dominica", 23},
    {u"Dominican Republic", 24},
    {u"Ecuador", 25},
    {u"El Salvador", 26},
    {u"French Guiana", 27},
    {u"Grenada", 28},
    {u"Guadeloupe", 29},
    {u"Guatemala", 30},
    {u"Guyana", 31},
    {u"Haiti", 32},
    {u"Honduras", 33},
    {u"Jamaica", 34},
    {u"Martinique", 35},
    {u"Mexico", 36},
    {u"Montserrat", 37},
    {u"Netherlands Antilles", 38},
    {u"Nicaragua", 39},
    {u"Panama", 40},
    {u"Paraguay", 41},
    {u"Peru", 42},
    {u"Saudi Arabia", 174},
    {u"Singapore", 153},
    {u"St. Kitts and Nevis", 43},
    {u"St. Lucia", 44},
    {u"St. Vincent and the Grenadines", 45},
    {u"Suriname", 46},
    {u"Trinidad and Tobago", 47},
    {u"Turks and Caicos Islands", 48},
    {u"U.A.E", 168},
    {u"United States", 49},
    {u"Uruguay", 50},
    {u"US Virgin Islands", 51},
    {u"Venezuela", 52},
}};

}

std::span<const Country> SelectableCountries()
{
    return Countries;
}

std::optional<Country> FindCountry(std::uint8_t code)
{
    auto found = std::find_if(Countries.begin(), Countries.end(), [code](const Country& country) { return country.code == code; });
    if (found == Countries.end())
        return std::nullopt;

    return *found;
}

}
