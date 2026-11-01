#include "Channels/ChannelSystem.h"

#include "Channels/ChannelError.h"
#include "Channels/ChannelFiles.h"
#include "Channels/NativeChannels.h"

#include "SystemMenu/HomeMenu/HomeMenuFiles.h"

#include "Formats/Font.h"

#include <format>

namespace Channels
{

namespace
{

// The two bitmap fonts of the console, under the names that the layouts give them (the same as MenuFonts).
constexpr std::string_view FirstFontFile = "wbf1.brfna";
constexpr std::string_view SecondFontFile = "wbf2.brfna";

// The big buttons of the HOME menu of a channel.
constexpr std::size_t WiiMenuButton = 0;
constexpr std::size_t ResetButton = 1;

ChannelSharedFiles SharedFilesOrFail(Ios::TitleId titleId)
{
    std::optional<ChannelSharedFiles> sharedFiles = SharedFilesOf(titleId);
    if (!sharedFiles)
        throw ChannelError(std::format("WeeOS has no program for the channel {}", titleId.ToString()));

    return *sharedFiles;
}

}

ChannelSystem::ChannelSystem(Graphics::GraphicsDevice& device, SystemMenu::TitleContents& contents, Ios::RemoteService& remoteService,
    Ios::NetworkConfigService& network, const ChannelStorage& storage, Ios::PowerService& powerService, Platform::ScreenShape screenShape,
    Ios::TitleId titleId)
    : runningTitle(titleId)
    , sharedFiles(SharedFilesOrFail(titleId))
    , power(powerService)
    , display(device, screenShape)
    , fader(display)
    , input(remoteService)
    , services{display, clock, fader, input, sounds, contents, network, wiiMenu, systemFonts.Fonts(), storage, stream}
{
    LoadSystemFonts(contents);
    channel = MakeChannel(titleId, services);

    homeButton = std::make_unique<SystemMenu::HomeButton>(display, [this, &contents] { return ReadHomeMenuFiles(contents); }, input,
        remoteService, channel->Hands(), fader, ConnectHomeButton());
    homeButton->Start();
}

void ChannelSystem::Update()
{
    sounds.StartNextStep();
    clock.Advance();
    input.Poll();
    AnswerFrontPanel();
    FollowHomeButtonPermission();

    // The HOME button is seen before the channel moves on, so a press stops the channel in the step it came.
    homeButton->Update();
    if (!homeButton->IsOpen())
        channel->Update();

    channel->Hands().Update(input);
    fader.Calculate();
    LeaveWhenAsked();
}

void ChannelSystem::Draw()
{
    display.BeginPicture();
    DrawScene();
}

// The HOME menu lies over the channel and draws the hands; while it is closed it draws the hands of the channel.
void ChannelSystem::DrawScene()
{
    channel->Draw();
    homeButton->Draw();
    fader.Draw();
}

std::optional<Ios::TitleId> ChannelSystem::TakeTitleToLaunch()
{
    return titleLaunch.Take();
}

ChannelServices& ChannelSystem::Services()
{
    return services;
}

Channel& ChannelSystem::RunningChannel()
{
    return *channel;
}

bool ChannelSystem::IsHomeMenuOpen() const
{
    return homeButton->IsOpen();
}

void ChannelSystem::LoadSystemFonts(SystemMenu::TitleContents& contents)
{
    if (!sharedFiles.fontContentIndex)
        return;

    Formats::U8Archive fontFiles = ReadArchiveContent(contents, *sharedFiles.fontContentIndex);
    Graphics::GraphicsDevice& device = display.Device();
    systemFonts.AddFont(device, Formats::ReadFont(FindFileOrFail(fontFiles, FirstFontFile)),
        {"RevoIpl_RodinNTLGPro_DB_32_I4.brfnt", "WiiBitmapFontType1.brfnt", FirstFontFile});
    systemFonts.AddFont(device, Formats::ReadFont(FindFileOrFail(fontFiles, SecondFontFile)),
        {"RevoIpl_UtrilloProGrecoStd_M_32_I4.brfnt", "WiiBitmapFontType2.brfnt", SecondFontFile});
}

SystemMenu::HomeMenuFiles ChannelSystem::ReadHomeMenuFiles(SystemMenu::TitleContents& contents) const
{
    Formats::U8Archive homeMenuContent = ReadArchiveContent(contents, sharedFiles.homeMenuContentIndex);
    return SystemMenu::ReadChannelHomeMenuFiles(homeMenuContent, sharedFiles.homeMenuFolder, sharedFiles.isHomeMenuPacked,
        sharedFiles.offeredHomeMenuButtons);
}

SystemMenu::HomeButtonConnections ChannelSystem::ConnectHomeButton()
{
    SystemMenu::HomeButtonConnections connections;
    connections.playSound = [this](SystemMenu::HomeMenuSound sound) { sounds.Request(SystemMenu::NameOfHomeMenuSound(sound)); };
    connections.textFonts = &systemFonts.Fonts();
    connections.chooseBigButton = [this](std::size_t bigButton) { LeaveThroughHomeMenu(bigButton); };
    return connections;
}

// The HOME menu has faded the picture to black: "Wii Menu" starts the System Menu, "Reset" starts the channel again (OSRestart).
void ChannelSystem::LeaveThroughHomeMenu(std::size_t bigButton)
{
    fader.SetCovered();
    hasAskedForMenu = true;
    if (bigButton == ResetButton)
        titleLaunch.Request(runningTitle);
    else if (bigButton == WiiMenuButton)
        titleLaunch.Request(Ios::SystemMenuTitleId);
}

// A channel can keep the HOME menu shut for a while; HOME then shows the picture that says so.
void ChannelSystem::FollowHomeButtonPermission()
{
    if (channel->AllowsHomeButton())
        homeButton->Enable();
    else
        homeButton->Disable();
}

// The SDK of the channels switches the console off for POWER and restarts it for RESET (CHN-27).
void ChannelSystem::AnswerFrontPanel()
{
    std::optional<Ios::PowerEvent> button = power.TryTakeEvent();
    if (!button)
        return;

    homeButton->HoldForFrontPanel();
    if (*button == Ios::PowerEvent::PowerButtonPressed)
        power.RequestShutdown();
    else
        power.RequestReset();
}

void ChannelSystem::LeaveWhenAsked()
{
    if (!wiiMenu.IsRequested() || hasAskedForMenu)
        return;

    if (fader.GetStatus() == SystemMenu::FaderStatus::Clear)
        fader.StartFadeOut();

    if (fader.GetStatus() != SystemMenu::FaderStatus::Covered)
        return;

    titleLaunch.Request(Ios::SystemMenuTitleId);
    hasAskedForMenu = true;
}

}
