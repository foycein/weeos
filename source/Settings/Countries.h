#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace Settings
{

// A country of the list that the Wii Settings page "Country" of the USA console offers.
struct Country
{
    std::u16string_view name;

    // The code that the console keeps in its location setting. The codes are the console's (WiiBrew, page Country Codes).
    std::uint8_t code;
};

// The countries of the page, in the order of the page: sorted by name, with the three that do not belong to the Americas (Saudi Arabia,
// Singapore and U.A.E.) where their names fall. The page of the original numbers the list from 8 on and the console's codes differ from
// these numbers behind Peru, so the list holds the code of each country.
std::span<const Country> SelectableCountries();

// The country with this code, or nothing when the page does not offer it.
std::optional<Country> FindCountry(std::uint8_t code);

}
