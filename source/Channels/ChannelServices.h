#pragma once

#include "SystemMenu/Display.h"
#include "SystemMenu/Fader.h"
#include "SystemMenu/InputManager.h"
#include "SystemMenu/MenuClock.h"
#include "SystemMenu/SoundRequests.h"
#include "SystemMenu/TitleContents.h"

#include "Ios/FileSystem/FileSystemService.h"
#include "Ios/Network/NetworkConfigService.h"
#include "Ios/SdCard/SdCardService.h"
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
// The storage of the console that a channel may use: the SD card slot and the file system of the NAND (the channel's own data folder).
struct ChannelStorage
{
    Ios::SdCardService& sdCard;
    Ios::FileSystemService& fileSystem;

    // The running title as the file system sees it: the owner of the channel's data folder.
    Ios::Caller caller;
};

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

    ChannelStorage storage;
};

}
