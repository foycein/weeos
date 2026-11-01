#pragma once

#include "Channels/Channel.h"
#include "Channels/ChannelServices.h"
#include "Channels/Manual/ManualViewer.h"
#include "Channels/NativeChannels.h"

#include "SystemMenu/HomeMenu/HomeButton.h"
#include "SystemMenu/MenuFonts.h"
#include "SystemMenu/TitleLaunchRequest.h"

#include "Graphics/GraphicsDevice.h"
#include "Ios/Power/PowerService.h"
#include "Ios/Remote/RemoteService.h"
#include "Ios/Title/TitleId.h"
#include "Ios/Title/TitleService.h"

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
// the channel stands still. Its "Wii Menu" starts the System Menu, its "Reset" starts the channel again (CHN-16), and its
// "Operations Guide" opens the channel's manual over the channel, which stands still until HOME closes the manual (CHN-13).
// The POWER button switches the console off and the RESET button starts it again (CHN-27).
// When the user closes the window, the question "Turn off WeeOS?" of the menu lies over the channel (AskToTurnOff).
class ChannelSystem
{
public:
    // Throws a ChannelError when WeeOS has no program for the title. The device, both contents and the services must outlive the
    // channel system. The contents of the System Menu are read only when the question "Turn off WeeOS?" first comes.
    // A channel that brings no fonts of its own draws its texts with the console's fonts, when they are given (ReadConsoleFontArchive).
    ChannelSystem(Graphics::GraphicsDevice& device, SystemMenu::TitleContents& contents, SystemMenu::TitleContents& systemMenuContents,
        Ios::RemoteService& remoteService, const ChannelNetwork& network, const ChannelStorage& storage, const ChannelAudio& audio,
        Ios::PowerService& powerService, Ios::TitleId titleId, Settings::PcClock consoleTime = Settings::ConsoleClock::ReadWindowsClock,
        std::optional<Formats::Bytes> consoleFontArchive = std::nullopt);
    ~ChannelSystem();

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

    // The manual while it is open, or nothing.
    ManualViewer* OpenManualViewer();

    // Asks "Turn off WeeOS?" over the channel; the channel stands still until the user answers, and Yes is the POWER button.
    void AskToTurnOff();
    bool IsAskingToTurnOff() const;

    // The display that the question is drawn on while it is open, with its own fonts. A test finds the buttons of the question there.
    const SystemMenu::Display* TurnOffQuestionDisplay() const;

private:
    // The dialog windows and the fonts of the System Menu, with a display of their own, for the question "Turn off WeeOS?".
    class TurnOffParts;

    void LoadSystemFonts(SystemMenu::TitleContents& contents, std::optional<Formats::Bytes> consoleFontArchive);
    SystemMenu::HomeMenuFiles ReadHomeMenuFiles(SystemMenu::TitleContents& contents) const;
    SystemMenu::HomeButtonConnections ConnectHomeButton();
    void LeaveThroughHomeMenu(std::size_t bigButton);
    void OpenChannelManual();
    void UpdateManual();
    void FollowHomeButtonPermission();
    void AnswerFrontPanel();
    void PressPowerButton();
    void RunStepOfTheTurnOffQuestion();
    void LeaveWhenAsked();

    Ios::TitleId runningTitle;
    Graphics::GraphicsDevice& graphicsDevice;
    SystemMenu::TitleContents& menuContents;
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
    std::unique_ptr<ManualViewer> manual;
    bool isClosingManual = false;
    std::unique_ptr<TurnOffParts> turnOff;
    SystemMenu::TitleLaunchRequest titleLaunch;
    bool hasAskedForMenu = false;
};

// The archive of the console's two bitmap fonts: content 5 of the System Menu, which titles share (CHN-77). Nothing when it is not there.
std::optional<Formats::Bytes> ReadConsoleFontArchive(const Ios::TitleService& titles);

}
