#pragma once

#include "SystemMenu/Display.h"
#include "SystemMenu/Fader.h"
#include "SystemMenu/InputManager.h"
#include "SystemMenu/MenuClock.h"
#include "SystemMenu/SoundRequests.h"
#include "SystemMenu/TitleContents.h"

#include "Ios/Caller.h"
#include "Ios/FileSystem/FileSystemService.h"
#include "Ios/Microphone/MicrophoneService.h"
#include "Ios/Network/NetworkConfigService.h"
#include "Ios/SdCard/SdCardService.h"
#include "Ios/Sockets/SocketService.h"
#include "Layout/TextFonts.h"
#include "Sound/WavePlayer.h"
#include "Settings/ConsoleClock.h"

#include <string>

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

// The network of a channel: the network configuration module and the sockets of IOS, and the address of the owner's server, which takes
// the place of Nintendo's servers (docs/DECISIONS.md, Online). The address is the setting "Server" of the settings of the PC.
struct ChannelNetwork
{
    Ios::NetworkConfigService& configuration;
    Ios::SocketService& sockets;
    std::string serverAddress;
};

// The storage of the console that a channel may use: the SD card slot and the file system of the NAND, where the channel is the
// user and the group of its title (its own data folder).
struct ChannelStorage
{
    Ios::SdCardService& sdCard;
    Ios::FileSystemService& fileSystem;

    // Who the channel is to the file system: the user and the group of its title.
    Ios::Caller fileSystemCaller;
};

// What a channel hears and plays beyond its sound archives: the Wii Speak accessory of IOS and the player of the waves that the
// channel makes itself (a recorded voice, a stream file).
struct ChannelAudio
{
    Ios::MicrophoneService& microphone;
    Sound::WavePlayer& waves;
};

// Everything a channel gets from the channel system: the same engines that the menu uses, the contents of its own title, the network
// and the storage.
struct ChannelServices
{
    SystemMenu::Display& display;
    const SystemMenu::MenuClock& clock;
    SystemMenu::Fader& fader;
    SystemMenu::InputManager& input;
    SystemMenu::SoundRequests& sounds;
    SystemMenu::TitleContents& contents;
    ChannelNetwork network;
    WiiMenuReturn& wiiMenu;

    // The two bitmap fonts of the console, wbf1.brfna and wbf2.brfna, from the channel's shared content of fonts.
    const Layout::TextFonts& systemFonts;

    ChannelStorage storage;
    ChannelAudio audio;

    // The time of the console's clock as the PC gives it: what a channel seeds its chances with and the time a Mii is made.
    Settings::PcClock consoleTime;
};

}
