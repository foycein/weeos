#pragma once

#include "Channels/Channel.h"
#include "Channels/ChannelServices.h"
#include "Channels/NativeChannels.h"

#include "SystemMenu/HomeMenu/HomeButton.h"
#include "SystemMenu/MenuFonts.h"
#include "SystemMenu/TitleLaunchRequest.h"

#include "Graphics/GraphicsDevice.h"
#include "Ios/Power/PowerService.h"
#include "Ios/Remote/RemoteService.h"
#include "Ios/Title/TitleId.h"
#include "Platform/ScreenShape.h"

#include <cstddef>
#include <memory>
#include <optional>

namespace Channels
{

// Runs the channel whose title the console has entered, as MenuSystem runs the menu: it makes the engines, the channel's program
// (NativeChannels) and runs one step and one picture at a time. When the channel asks for the Wii Menu, the picture fades to black
// and the System Menu is asked for.
//
// HOME opens the HOME Button Menu of the channel's own files, as the HBM library inside every channel program does. While it is open
// the channel stands still. Its "Wii Menu" starts the System Menu, its "Reset" starts the channel again (CHN-16).
// The POWER button switches the console off and the RESET button starts it again (CHN-27).
class ChannelSystem
{
public:
    // Throws a ChannelError when WeeOS has no program for the title. The device, the contents and the services must outlive the channel system.
    ChannelSystem(Graphics::GraphicsDevice& device, SystemMenu::TitleContents& contents, Ios::RemoteService& remoteService,
        const ChannelNetwork& network, const ChannelStorage& storage, Ios::PowerService& powerService, Platform::ScreenShape screenShape,
        Ios::TitleId titleId);

    // One step of the console's picture, 1001/60000 of a second.
    void Update();

    // Starts a new picture of the screen and draws it. The program shows it with Present.
    void Draw();

    // Draws the picture into the picture that is open, for example an offscreen picture.
    void DrawScene();

    // The title that the channel asked for in the steps since the last call: the System Menu, or the channel itself after "Reset".
    std::optional<Ios::TitleId> TakeTitleToLaunch();

    ChannelServices& Services();
    Channel& RunningChannel();
    bool IsHomeMenuOpen() const;

private:
    void LoadSystemFonts(SystemMenu::TitleContents& contents);
    SystemMenu::HomeMenuFiles ReadHomeMenuFiles(SystemMenu::TitleContents& contents) const;
    SystemMenu::HomeButtonConnections ConnectHomeButton();
    void LeaveThroughHomeMenu(std::size_t bigButton);
    void FollowHomeButtonPermission();
    void AnswerFrontPanel();
    void LeaveWhenAsked();

    Ios::TitleId runningTitle;
    ChannelSharedFiles sharedFiles;
    Ios::PowerService& power;
    SystemMenu::Display display;
    SystemMenu::MenuClock clock;
    SystemMenu::Fader fader;
    SystemMenu::InputManager input;
    SystemMenu::SoundRequests sounds;
    WiiMenuReturn wiiMenu;
    SystemMenu::MenuFonts systemFonts;
    ChannelServices services;
    std::unique_ptr<Channel> channel;
    std::unique_ptr<SystemMenu::HomeButton> homeButton;
    SystemMenu::TitleLaunchRequest titleLaunch;
    bool hasAskedForMenu = false;
};

}
