#pragma once

#include "SystemMenu/Display.h"
#include "SystemMenu/Fader.h"
#include "SystemMenu/InputManager.h"
#include "SystemMenu/MenuClock.h"
#include "SystemMenu/SoundRequests.h"
#include "SystemMenu/TitleContents.h"

#include "Ios/Network/NetworkConfigService.h"
#include "Layout/TextFonts.h"

namespace Channels
{

// A channel asks for the Wii Menu, as its own "Wii Menu" buttons and the HOME button do. The channel system fades the picture out
// and then starts the System Menu again, as the console does when a channel returns to the menu.
class WiiMenuReturn
{
public:
    void Request()
    {
        isRequested = true;
    }

    bool IsRequested() const
    {
        return isRequested;
    }

private:
    bool isRequested = false;
};

// Everything a channel gets from the channel system: the same engines that the menu uses, the contents of its own title and the
// network configuration module of IOS.
struct ChannelServices
{
    SystemMenu::Display& display;
    const SystemMenu::MenuClock& clock;
    SystemMenu::Fader& fader;
    SystemMenu::InputManager& input;
    SystemMenu::SoundRequests& sounds;
    SystemMenu::TitleContents& contents;
    Ios::NetworkConfigService& network;
    WiiMenuReturn& wiiMenu;

    // The two bitmap fonts of the console, wbf1.brfna and wbf2.brfna, from the channel's shared content of fonts.
    const Layout::TextFonts& systemFonts;
};

}
