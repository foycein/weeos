#pragma once

#include "Ios/IosError.h"

namespace SystemMenu
{

// The reasons for which the menu shows the error screen and stops. The number of a reason is the number
// of its text among the error texts of the main program, and the code that the original passes to its error screen (0x81356514).
enum class FatalError
{
    // The disc drive failed. The text asks the user to eject the disc.
    DiscError = 0,

    // The NAND chip failed.
    SystemMemoryDamaged = 1,

    // A file of the menu, a title or a ticket is missing or not what it should be. The most common reason by far.
    SystemFilesCorrupted = 2,

    // A channel cannot start.
    ChannelCannotBeUsed = 3,

    // Error 003: the console has the common key of Korean consoles, and this System Menu is not a Korean one (0x8133400C asks the title
    // service at start).
    UnauthorizedDevice = 4,
};

// The reason that goes with a failed call of a service of IOS.
FatalError FatalErrorOf(Ios::IosError error);

}
