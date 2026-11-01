#pragma once

#include "SystemMenu/MenuConsole.h"

#include "Input/RemoteSource.h"

#include <array>
#include <string_view>

namespace SystemMenu
{

// The file of the system configuration of the console.
constexpr std::string_view SystemConfigurationPath = "/shared2/sys/SYSCONF";

// What the Wii Menu takes from the system configuration (SYSCONF).
struct MenuSettings
{
    // The language of the console, numbered as the console numbers them: English 1, French 3, Spanish 4 (setting IPL.LNG).
    // A console of the USA shows its menu in English, French or Spanish. Any other language of the file means English.
    int languageNumber = 1;

    // True when the console is set to 16:9 (setting IPL.AR). Anything else is 4:3.
    bool isWideScreen = false;

    // Where the sensor bar stands (setting BT.BAR).
    Input::SensorBarPosition sensorBarPosition = Input::SensorBarPosition::BelowScreen;
};

// Reads the settings from the system configuration of the console. A console without the file has the settings that the struct starts with.
// Throws a FormatError when the file is there and damaged.
MenuSettings ReadMenuSettings(MenuConsole& console);

// The languages in which a console of the USA can show its menu, in the order in which the menu tries them when a channel
// has no name in the language that the console has set. The first one is English.
constexpr std::array<int, 3> LanguagesOfTheUsa = {1, 3, 4};

}
