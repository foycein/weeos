#pragma once

#include "Ios/IosError.h"

namespace SystemMenu
{

// The reasons for which the menu shows the error screen and stops (docs/audit/SystemMenu.md). The number of a reason is the number
// of its text among the error texts of the main program.
enum class FatalError
{
    // The disc drive failed. The text asks the user to eject the disc.
    DiscError = 0,

    // The NAND chip failed.
    SystemMemoryDamaged = 1,

    // A file of the menu, a title or a ticket is missing or not what it should be.
    SystemFilesCorrupted = 2,

    // A channel cannot start.
    ChannelCannotBeUsed = 3,

    // A ticket or a signature says that the title is not for this console.
    UnauthorizedDevice = 4,
};

// The reason that goes with a failed call of a service of IOS.
FatalError FatalErrorOf(Ios::IosError error);

}
