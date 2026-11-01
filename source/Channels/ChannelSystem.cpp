#include "Channels/ChannelSystem.h"

#include "Channels/ChannelError.h"
#include "Channels/ChannelFiles.h"
#include "Channels/NativeChannels.h"

#include "SystemMenu/HomeMenu/HomeMenuFiles.h"
#include "SystemMenu/MenuFiles.h"
#include "SystemMenu/MenuFonts.h"
#include "SystemMenu/TurnOffQuestion.h"

#include "Formats/Font.h"
#include "Formats/Yaz0.h"

#include <format>
#include <utility>
#include <vector>

namespace Channels
{

namespace
{

// The two bitmap fonts of the console, under the names that the layouts give them (the same as MenuFonts).
constexpr std::string_view FirstFontFile = "wbf1.brfna";
constexpr std::string_view SecondFontFile = "wbf2.brfna";

// The System Menu keeps the archive of the two fonts in its content 5 (MenuFonts).
constexpr std::uint16_t ConsoleFontsContentIndex = 5;

// The big buttons of the HOME menu of a channel.
constexpr std::size_t WiiMenuButton = 0;
constexpr std::size_t ResetButton = 1;
constexpr std::size_t ManualButton = 2;

ChannelSharedFiles SharedFilesOrFail(Ios::TitleId titleId)
{
    std::optional<ChannelSharedFiles> sharedFiles = SharedFilesOf(titleId);
    if (!sharedFiles)
        throw ChannelError(std::format("WeeOS has no program for the channel {}", titleId.ToString()));

    return *sharedFiles;
}

}

class ChannelSystem::TurnOffParts
{
public:
    TurnOffParts(Graphics::GraphicsDevice& device, SystemMenu::TitleContents& menuContents, SystemMenu::SoundRequests& sounds)
        : files(menuContents)
        , fonts(device, menuContents, files)
        , display(device)
    {
        display.SetTextFonts(fonts.Fonts());
        question = std::make_unique<SystemMenu::TurnOffQuestion>(display, files, sounds);
    }

    SystemMenu::TurnOffQuestion& Question()
    {
        return *question;
    }

    const SystemMenu::Display& Display() const
    {
        return display;
    }

    void Draw()
    {
        display.ForgetDrawnLayouts();
        question->Draw();
    }

private:
    SystemMenu::MenuFiles files;
    SystemMenu::MenuFonts fonts;
    SystemMenu::Display display;
    std::unique_ptr<SystemMenu::TurnOffQuestion> question;
};

ChannelSystem::ChannelSystem(Graphics::GraphicsDevice& device, SystemMenu::TitleContents& contents,
    SystemMenu::TitleContents& systemMenuContents, Ios::RemoteService& remoteService, const ChannelNetwork& network,
    const ChannelStorage& storage, const ChannelAudio& audio, Ios::PowerService& powerService, Ios::TitleId titleId, Settings::PcClock consoleTime,
    std::optional<Formats::Bytes> consoleFontArchive, const ChannelPc& pc)
    : runningTitle(titleId)
    , graphicsDevice(device)
    , menuContents(systemMenuContents)
    , sharedFiles(SharedFilesOrFail(titleId))
    , power(powerService)
    , display(device)
    , fader(display)
    , input(remoteService)
    , services{display, clock, fader, input, sounds, contents, network, wiiMenu, systemFonts.Fonts(), storage, audio, std::move(consoleTime), pc}
{
    LoadSystemFonts(contents, std::move(consoleFontArchive));
    channel = MakeChannel(titleId, services);

    homeButton = std::make_unique<SystemMenu::HomeButton>(display, [this, &contents] { return ReadHomeMenuFiles(contents); }, input,
        remoteService, channel->Hands(), fader, ConnectHomeButton());
    homeButton->Start();
}

ChannelSystem::~ChannelSystem() = default;

void ChannelSystem::Update()
{
    sounds.StartNextStep();
    clock.Advance();
    input.Poll();
    AnswerFrontPanel();

    if (IsAskingToTurnOff())
    {
        RunStepOfTheTurnOffQuestion();
        return;
    }
    FollowHomeButtonPermission();

    if (manual)
    {
        UpdateManual();
        channel->Hands().Update(input);
        fader.Calculate();
        return;
    }

    // The HOME button is seen before the channel moves on, so a press stops the channel in the step it came.
    bool wasHomeMenuOpen = homeButton->IsOpen();
    homeButton->Update();
    if (!homeButton->IsOpen())
        channel->Update();

    // The HOME menu pauses the sounds of the channel while it is open and fades the console out with its picture.
    if (homeButton->IsOpen() && !wasHomeMenuOpen)
        sounds.PauseAllSounds();
    if (!homeButton->IsOpen() && wasHomeMenuOpen)
        sounds.ResumeAllSounds();
    sounds.ChangeConsoleVolume(homeButton->SoundVolume());

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
    if (manual)
        manual->Draw();
    else
        channel->Draw();

    homeButton->Draw();
    if (IsAskingToTurnOff())
    {
        turnOff->Draw();
        channel->Hands().Draw();
    }
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

ManualViewer* ChannelSystem::OpenManualViewer()
{
    return manual.get();
}

void ChannelSystem::AskToTurnOff()
{
    if (!turnOff)
        turnOff = std::make_unique<TurnOffParts>(graphicsDevice, menuContents, sounds);

    turnOff->Question().Ask();
}

bool ChannelSystem::IsAskingToTurnOff() const
{
    return turnOff && turnOff->Question().IsOpen();
}

const SystemMenu::Display* ChannelSystem::TurnOffQuestionDisplay() const
{
    if (!IsAskingToTurnOff())
        return nullptr;

    return &turnOff->Display();
}

void ChannelSystem::RunStepOfTheTurnOffQuestion()
{
    bool isYes = turnOff->Question().Update(input);
    if (isYes)
        PressPowerButton();

    channel->Hands().Update(input);
    fader.Calculate();
}

void ChannelSystem::LoadSystemFonts(SystemMenu::TitleContents& contents, std::optional<Formats::Bytes> consoleFontArchive)
{
    std::optional<Formats::U8Archive> fontFiles;
    if (sharedFiles.fontContentIndex)
        fontFiles = ReadArchiveContent(contents, *sharedFiles.fontContentIndex);
    else if (consoleFontArchive)
        fontFiles = Formats::ReadU8Archive(*consoleFontArchive);
    else
        return;

    Graphics::GraphicsDevice& device = display.Device();
    systemFonts.AddFont(device, Formats::ReadFont(FindFileOrFail(*fontFiles, FirstFontFile)),
        {"RevoIpl_RodinNTLGPro_DB_32_I4.brfnt", "WiiBitmapFontType1.brfnt", FirstFontFile});
    systemFonts.AddFont(device, Formats::ReadFont(FindFileOrFail(*fontFiles, SecondFontFile)),
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
    if (bigButton == ManualButton)
    {
        OpenChannelManual();
        return;
    }

    hasAskedForMenu = true;
    if (bigButton == ResetButton)
        titleLaunch.Request(runningTitle);
    else if (bigButton == WiiMenuButton)
        titleLaunch.Request(Ios::SystemMenuTitleId);
}

// "Operations Guide" of the HOME menu: the menu has made the picture black, and the manual fades in from black.
void ChannelSystem::OpenChannelManual()
{
    if (!sharedFiles.manual)
        return;

    manual = OpenManual(services, *sharedFiles.manual);
    isClosingManual = false;
    homeButton->ShowPictureAgain();
    fader.StartFadeIn();
}

// HOME in the manual fades it out; then the channel fades in where it stood.
void ChannelSystem::UpdateManual()
{
    if (!isClosingManual)
        manual->Update();

    bool canStartClosing = manual->WantsToClose() && !isClosingManual && fader.GetStatus() == SystemMenu::FaderStatus::Clear;
    if (canStartClosing)
    {
        fader.StartFadeOut();
        isClosingManual = true;
    }

    if (isClosingManual && fader.GetStatus() == SystemMenu::FaderStatus::Covered)
    {
        manual.reset();
        isClosingManual = false;
        fader.StartFadeIn();
    }
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

    if (*button == Ios::PowerEvent::PowerButtonPressed)
    {
        PressPowerButton();
        return;
    }

    homeButton->HoldForFrontPanel();
    power.RequestReset();
}

void ChannelSystem::PressPowerButton()
{
    homeButton->HoldForFrontPanel();
    power.RequestShutdown();
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

std::optional<Formats::Bytes> ReadConsoleFontArchive(const Ios::TitleService& titles)
{
    Ios::Result<std::vector<Ios::TicketView>> ticketViews = titles.ListTicketViews(Ios::SystemMenuTitleId);
    if (ticketViews.Failed() || ticketViews.Value().empty())
        return std::nullopt;

    Ios::Result<Ios::ContentStream> opened = titles.OpenContent(Ios::SystemMenuTitleId, ticketViews.Value().front(), ConsoleFontsContentIndex);
    if (opened.Failed())
        return std::nullopt;

    Ios::ContentStream& stream = opened.Value();
    return stream.Read(stream.GetLength());
}

}
