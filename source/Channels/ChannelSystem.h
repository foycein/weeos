#pragma once

#include "Channels/Channel.h"
#include "Channels/ChannelServices.h"

#include "SystemMenu/TitleLaunchRequest.h"

#include "Graphics/GraphicsDevice.h"
#include "Ios/Power/PowerService.h"
#include "Ios/Remote/RemoteService.h"
#include "Ios/Title/TitleId.h"
#include "Platform/ScreenShape.h"

#include <memory>
#include <optional>

namespace Channels
{

// The HOME button of the Wii Remote and of the Classic Controller.
constexpr SystemMenu::ButtonSet HomeButtons = {Input::RemoteButtons::Home, Input::ClassicButtons::Home};

// Runs the channel whose title the console has entered, as MenuSystem runs the menu: it makes the engines, the channel's program
// (NativeChannels) and runs one step and one picture at a time. When the channel asks for the Wii Menu, the picture fades to black
// and the System Menu is asked for.
//
// The POWER button switches the console off and the RESET button starts it again (CHN-27). The HOME button returns to the Wii Menu
// as the channel's own buttons do, while the channel allows it. The original opens the HOME Button Menu there,
// whose "Wii Menu" does the same; WeeOS has no HOME Button Menu yet (docs/audit/Channels.md).
class ChannelSystem
{
public:
    // Throws a ChannelError when WeeOS has no program for the title. The device, the contents and the services must outlive the channel system.
    ChannelSystem(Graphics::GraphicsDevice& device, SystemMenu::TitleContents& contents, Ios::RemoteService& remoteService,
        Ios::NetworkConfigService& network, Ios::PowerService& powerService, Platform::ScreenShape screenShape, Ios::TitleId titleId);

    // One step of the console's picture, 1001/60000 of a second.
    void Update();

    // Starts a new picture of the screen and draws it. The program shows it with Present.
    void Draw();

    // Draws the picture into the picture that is open, for example an offscreen picture.
    void DrawScene();

    // The title that the channel asked for in the steps since the last call: the System Menu, once the picture is black.
    std::optional<Ios::TitleId> TakeTitleToLaunch();

    ChannelServices& Services();
    Channel& RunningChannel();

private:
    void AnswerFrontPanel();
    void LeaveWhenAsked();

    Ios::PowerService& power;
    SystemMenu::Display display;
    SystemMenu::MenuClock clock;
    SystemMenu::Fader fader;
    SystemMenu::InputManager input;
    SystemMenu::SoundRequests sounds;
    WiiMenuReturn wiiMenu;
    ChannelServices services;
    std::unique_ptr<Channel> channel;
    SystemMenu::TitleLaunchRequest titleLaunch;
    bool hasAskedForMenu = false;
};

}
