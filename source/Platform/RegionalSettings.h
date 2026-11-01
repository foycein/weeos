#pragma once

#include <string>

namespace Platform
{

// What Windows says about the place of the PC: its country and how it writes the time of day. WeeOS follows it (docs/DECISIONS.md,
// Settings): the country of the console and the clock of the Wii Menu.
struct RegionalSettings
{
    // The two letters of the country (ISO 3166), for example "US". Empty when Windows has none.
    std::string countryLetters = "US";

    // 24 hours ("HH:mm") or 12 hours with AM and PM ("h:mm tt").
    bool usesTwentyFourHourClock = false;

    // A 0 in front of an hour below 10 ("HH" and "hh" in the format of Windows).
    bool showsLeadingZeroOfHours = false;
};

// The regional settings of the user who runs WeeOS. A setting Windows cannot tell keeps the value of a PC in the USA.
RegionalSettings ReadRegionalSettings();

}
