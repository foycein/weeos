#pragma once

#include "SystemMenu/NandFiles.h"

#include "Formats/SystemConfiguration.h"

#include <optional>
#include <string>

namespace SystemMenu
{

// The parental controls of the console, as the setting IPL.PC keeps them.
struct ParentalControl
{
    bool isOn = false;

    // The four characters of the PIN. Empty when none is set.
    std::string pin;
};

// The settings of the console that the menu reads. They live in the file /shared2/sys/SYSCONF. The file is read again for every
// question, so a change that another part of the menu has written is seen at once.
class MenuSettings
{
public:
    // The files must stay alive as long as this object.
    explicit MenuSettings(NandFiles& nandFiles);

    // The first-time setup of the console is done (the setting IPL.CD2). A missing or damaged file means it is not.
    bool IsConsoleSetUp();

    // The update type of the console (the setting IPL.UPT), 0 when there is none.
    int UpdateType();

    // Nothing when the settings have no parental controls.
    std::optional<ParentalControl> ReadParentalControl();

    // What the parental controls restrict besides games: the News Channel (bit 0 of NET.CTPC) and the Internet Channel (WWW.RST).
    bool IsNewsChannelRestricted();
    bool IsInternetChannelRestricted();

private:
    std::optional<Formats::SystemConfiguration> ReadConfiguration();

    NandFiles& files;
};

}
