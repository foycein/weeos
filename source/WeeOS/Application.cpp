#include "WeeOS/Application.h"

#include "Channels/ChannelSystem.h"
#include "Channels/NativeChannels.h"
#include "Input/VirtualRemote.h"
#include "Ios/Keyboard/PcKeyboard.h"
#include "Platform/FilePicker.h"
#include "Platform/MessagePump.h"
#include "SystemMenu/BackgroundServices/StartServices.h"
#include "SystemMenu/DataManagement/DataManagementServices.h"
#include "SystemMenu/DataManagement/MenuCertificates.h"
#include "SystemMenu/IosNandFiles.h"
#include "SystemMenu/MenuSound.h"
#include "SystemMenu/MenuStateFile.h"
#include "SystemMenu/OwnTitleContents.h"
#include "SystemMenu/ScreenManager.h"
#include "SystemMenu/WiiMenu/IosMenuConsole.h"
#include "WeeOS/NewConsole.h"
#include "WeeOS/WiiSettingsServices.h"

#include <format>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace WeeOS
{

namespace
{

constexpr std::wstring_view WindowTitle = L"WeeOS";
constexpr std::wstring_view NandPickerTitle = L"Import your Wii: choose nand.bin of its BootMii backup";
constexpr std::wstring_view NandImageFileName = L"nand.bin";

}

namespace
{

// A channel starts when WeeOS has a program for it and its IOS is installed. A new console without the IOS of a channel
// (docs/audit/Portable.md) shows the menu's message that the channel cannot be used, as for a channel without a program.
bool CanStart(const Ios::TitleService& titles, Ios::TitleId titleId)
{
    if (!Channels::HasNativeProgram(titleId))
        return false;

    Ios::Result<Formats::TitleMetadata> metadata = titles.ReadTitleMetadata(titleId);
    if (metadata.Failed())
        return false;

    Ios::TitleId iosTitleId = Ios::TitleId::FromValue(metadata.Value().requiredSystemVersion);
    return titles.ReadTitleMetadata(iosTitleId).Succeeded();
}

}

std::optional<std::filesystem::path> PickNandWithWindows()
{
    return Platform::PickFile(std::wstring(NandPickerTitle), std::wstring(NandImageFileName));
}

// The program of the title that runs, the menu or a channel, with everything it reads through the services of the IOS that runs, and
// the keys of the PC that go to those services.
// The window sends its events to the keys, the keys to the keyboard service, and the keyboard service to the virtual remote.
class Application::Session
{
public:
    Session(Ios::IosServices& iosServices, Ios::TitleId runningTitle, Platform::Window& programWindow,
        Graphics::GraphicsDevice& graphicsDevice, Platform::HostCommandListener& commandListener, ProgramSound& programSound,
        const HostEnvironment& host, SystemMenu::ConsoleImport& importOfAWii)
        : window(programWindow)
        , consoleImport(importOfAWii)
        , sound(programSound)
        , titleContents(iosServices.GetTitles())
        , nandFiles(iosServices.GetFileSystem(), iosServices.GetTitles())
        , wiiSettings(iosServices, programWindow, iosServices.GetRemote().GetVirtualRemote(), programSound, host.clock, host.pcSettingsFile)
        , wiiMenuConsole(iosServices, host.clock)
        , keyboard(iosServices.GetKeyboard(), iosServices.GetRemote().GetVirtualRemote())
        , hostKeys(keyboard, commandListener)
        , virtualRemote(iosServices.GetRemote().GetVirtualRemote())
    {
        if (runningTitle == Ios::SystemMenuTitleId)
            StartMenu(iosServices, graphicsDevice);
        else
            StartChannel(iosServices, graphicsDevice, runningTitle);

        window.SetListener(&hostKeys);
    }

    ~Session()
    {
        sound.StopAllSounds();
        sound.StopTitleArchives();
        window.SetListener(nullptr);
    }

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    // Nothing while a channel runs.
    SystemMenu::MenuSystem* Menu()
    {
        return menu.get();
    }

    // Nothing while the menu runs.
    Channels::ChannelSystem* Channel()
    {
        return channel.get();
    }

    // The virtual remote has no speaker: the sounds that the program sends to its speaker are dropped.
    void Update()
    {
        if (menu)
        {
            menu->Update();
            PlaySoundsOfTheStep();
        }
        else
        {
            channel->Update();
            PlayChannelSoundsOfTheStep();
        }

        virtualRemote.TakeSpeakerSounds();
    }

    void Draw()
    {
        if (menu)
            menu->Draw();
        else
            channel->Draw();
    }

    std::optional<Ios::TitleId> TakeTitleToLaunch()
    {
        if (menu)
            return menu->TakeTitleToLaunch();

        return channel->TakeTitleToLaunch();
    }

    Input::VirtualRemote& VirtualRemote()
    {
        return virtualRemote;
    }

private:
    void StartMenu(Ios::IosServices& iosServices, Graphics::GraphicsDevice& graphicsDevice)
    {
        wiiSettings.FlowServices().consoleImport = &consoleImport;
        menu = std::make_unique<SystemMenu::MenuSystem>(graphicsDevice, titleContents, nandFiles, iosServices.GetRemote(),
            iosServices.GetPower(), wiiSettings.PictureShape(), wiiMenuConsole, &wiiSettings.FlowServices(), &consoleImport);
        SystemMenu::RunStartServices(iosServices.GetFileSystem(), iosServices.GetTitles());
        wiiSettings.UseDisplay(menu->Services().display);
        menu->UsePhysicalKeyboard(iosServices.GetKeyboard());
        Ios::TitleService& titles = iosServices.GetTitles();
        menu->Services().titleLaunch.SetProgramCheck([&titles](Ios::TitleId titleId) { return CanStart(titles, titleId); });

        dataManagement = std::make_unique<SystemMenu::DataManagementServices>(SystemMenu::DataManagementServices{iosServices.GetTitles(),
            iosServices.GetFileSystem(), iosServices.GetSdCard(), iosServices.GetConsoleIdentity(),
            SystemMenu::ReadMenuCertificates(menu->Services().program)});
        menu->Services().dataManagement = dataManagement.get();

        Sound::SoundSystem* soundSystem = sound.Start(menu->Services().files.ReadFile(SystemMenu::MenuSoundArchivePath));
        if (soundSystem != nullptr)
            menuSound.emplace(*soundSystem);
    }

    // What the screens asked to hear in the step that has just run, in the sound mode of the console's settings, which the Sound page
    // of Wii Settings changes at once.
    void PlaySoundsOfTheStep()
    {
        sound.SetSoundMode(wiiSettings.SoundMode());
        if (menuSound)
            menuSound->PlayRequests(menu->Services().sounds);
    }

    void StartChannel(Ios::IosServices& iosServices, Graphics::GraphicsDevice& graphicsDevice, Ios::TitleId runningTitle)
    {
        Channels::ChannelStorage storage{iosServices.GetSdCard(), iosServices.GetFileSystem(), iosServices.GetTitles().GetCaller()};
        std::string serverAddress = wiiSettings.FlowServices().pcSettings.Values().serverAddress;
        Channels::ChannelNetwork network{iosServices.GetNetworkConfig(), iosServices.GetSockets(), serverAddress};
        channel = std::make_unique<Channels::ChannelSystem>(graphicsDevice, titleContents, iosServices.GetRemote(),
            network, storage, iosServices.GetPower(), wiiSettings.PictureShape(), runningTitle,
            Channels::ReadConsoleFontArchive(iosServices.GetTitles()));

        std::vector<Sound::SoundSystem*> channelSounds;
        for (const Formats::Bytes& archive : Channels::ReadSoundArchives(runningTitle, titleContents))
        {
            Sound::SoundSystem* soundSystem = sound.StartTitleArchive(archive);
            if (soundSystem != nullptr)
                channelSounds.push_back(soundSystem);
        }

        if (!channelSounds.empty())
            channelSound.emplace(channelSounds);
    }

    // A channel asks for its sounds by name, as the menu does; each one plays from the first of the channel's archives that has it.
    void PlayChannelSoundsOfTheStep()
    {
        sound.SetSoundMode(wiiSettings.SoundMode());
        if (channelSound)
            channelSound->PlayRequests(channel->Services().sounds);
    }

    Platform::Window& window;
    SystemMenu::ConsoleImport& consoleImport;
    ProgramSound& sound;
    SystemMenu::OwnTitleContents titleContents;
    SystemMenu::IosNandFiles nandFiles;
    WiiSettingsServices wiiSettings;
    SystemMenu::IosMenuConsole wiiMenuConsole;

    // Declared before the menu, which keeps a pointer to it, so that the menu goes first.
    std::unique_ptr<SystemMenu::DataManagementServices> dataManagement;
    std::unique_ptr<SystemMenu::MenuSystem> menu;
    std::unique_ptr<Channels::ChannelSystem> channel;
    Ios::PcKeyboard keyboard;
    Platform::HostKeys hostKeys;
    Input::VirtualRemote& virtualRemote;
    std::optional<SystemMenu::MenuSound> menuSound;
    std::optional<SystemMenu::MenuSound> channelSound;
};

Application::Application(const Ios::IosFolders& iosFolders, bool isSilent, HostEnvironment hostEnvironment)
    : folders(iosFolders)
    , host(std::move(hostEnvironment))
    , importer(iosFolders, host.pickNand)
    , console(iosFolders, host.isPcConnected)
    , window(WindowTitle)
    , device(window)
    , sound(isSilent)
    , pacer(timer.ElapsedSeconds())
{
    PrepareConsoleFolders();
    console.PowerOn();
    StartSession();
}

// The SD card is always in its slot: its folder is made when it is not there. A NAND folder without a console gets a new one,
// whose first start offers the import of the person's own Wii.
void Application::PrepareConsoleFolders()
{
    if (!folders.sdCardFolder.empty())
        std::filesystem::create_directories(folders.sdCardFolder);

    if (HasConsole(folders))
        return;

    std::cout << "This is the first start: WeeOS makes a new console.\n";
    MakeNewConsole(folders, host.wiiFilesFolder);
    importer.OfferAtStart();
}

Application::~Application() = default;

void Application::StartSession()
{
    Platform::HostCommandListener& commandListener = *this;
    session = std::make_unique<Session>(console.GetServices(), RunningTitle(), window, device, commandListener, sound, host, importer);
    StopOnTheKeyOfKoreanConsoles();
}

// The System object of the original asks the title service this when the menu starts (0x8133400C). A failed question is no error.
void Application::StopOnTheKeyOfKoreanConsoles()
{
    Ios::Result<bool> hasKoreanKey = console.GetServices().GetTitles().CheckHasKoreanKey();
    if (session->Menu() != nullptr && hasKoreanKey.Succeeded() && hasKoreanKey.Value())
        session->Menu()->ShowFatalError(SystemMenu::FatalError::UnauthorizedDevice);
}

void Application::Run()
{
    while (!isPoweredOff)
        ShowNextPicture();
}

void Application::ShowNextPicture()
{
    Platform::ProcessPendingMessages();
    ReportCloseOfTheWindowAsPowerButton();

    if (isPoweredOff)
        return;

    if (window.IsMinimized())
    {
        Platform::WaitForMessage();
        return;
    }

    RunDueSteps();
    CarryOutRequestsOfTheMenu();
    StartImportedConsole();
    ReportProblems();
    ShowPicture();
}

void Application::RunOneStep()
{
    Platform::ProcessPendingMessages();
    ReportCloseOfTheWindowAsPowerButton();

    if (isPoweredOff)
        return;

    session->VirtualRemote().SetWindowSize(window.ClientSize());
    session->Update();
    CarryOutRequestsOfTheMenu();
    StartImportedConsole();
    ReportProblems();
}

void Application::ShowPicture()
{
    if (isPoweredOff || window.IsMinimized())
        return;

    session->Draw();
    device.Present();
}

// The window that is closed is the POWER button. A window that cannot be seen cannot show the picture that fades out, so it powers off at once.
void Application::ReportCloseOfTheWindowAsPowerButton()
{
    if (!window.IsCloseRequested() || hasReportedClose)
        return;

    hasReportedClose = true;
    if (window.IsMinimized())
        isPoweredOff = true;
    else
        console.GetServices().GetPower().ReportButtonPress(Ios::PowerEvent::PowerButtonPressed);
}

void Application::RunDueSteps()
{
    session->VirtualRemote().SetWindowSize(window.ClientSize());

    int dueSteps = pacer.TakeDueSteps(timer.ElapsedSeconds());
    for (int step = 0; step < dueSteps; ++step)
        session->Update();
}

void Application::CarryOutRequestsOfTheMenu()
{
    std::optional<Ios::TitleId> title = session->TakeTitleToLaunch();
    if (title)
    {
        LaunchTitle(*title);
        return;
    }

    Ios::EventQueue<Ios::PowerRequest>& requests = console.GetServices().GetPower().Requests();

    std::optional<Ios::PowerRequest> request = requests.TryTake();
    if (!request)
        return;

    if (*request == Ios::PowerRequest::Shutdown)
        isPoweredOff = true;
    else
        ResetConsole();
}

// The whole console starts again, from boot2 on (the HotReset of the original). IOS starts again, so everything that holds a service
// goes first and the new session takes the new services.
void Application::ResetConsole()
{
    session.reset();
    console.PowerOff();
    console.PowerOn();
    StartSession();
    ForgetReportedProblems();
}

// An import of a Wii that has finished takes the place of the console: IOS stops, the folders change and the console powers on again.
void Application::StartImportedConsole()
{
    if (isPoweredOff || !importer.TakeFinishedImport())
        return;

    session.reset();
    console.PowerOff();
    importer.PutImportInPlace();
    console.PowerOn();
    StartSession();
    ForgetReportedProblems();
}

// IOS starts again with the IOS of the title, as after ES_LaunchTitle. The ticket is the first of the title, as the original takes it.
// A channel that WeeOS has no program for is not started: the program that runs goes on.
void Application::LaunchTitle(Ios::TitleId titleId)
{
    if (titleId != Ios::SystemMenuTitleId && !Channels::HasNativeProgram(titleId))
    {
        std::cout << std::format("WeeOS has no program for the title {} yet. The program that runs goes on.\n", titleId.ToString());
        return;
    }

    Ios::Result<std::vector<Ios::TicketView>> ticketViews = console.GetServices().GetTitles().ListTicketViews(titleId);
    if (ticketViews.Failed() || ticketViews.Value().empty())
        throw std::runtime_error(std::format("The title {} has no ticket and cannot start", titleId.ToString()));

    Ios::TicketView ticketView = ticketViews.Value().front();
    bool isChannelReturning = session->Menu() == nullptr && titleId == Ios::SystemMenuTitleId;
    session.reset();
    console.LaunchTitle(titleId, ticketView);
    if (isChannelReturning)
        RecordReturnToTheMenu();

    StartSession();
    ForgetReportedProblems();
}

// A channel that goes back to the menu leaves the state of a return in the menu's state.dat, as the operating system of a title does
// (OSReturnToMenu writes the type 3), so that the menu shows Back to Wii Menu and not Health and Safety. WeeOS writes it as the menu,
// right after IOS has started the menu again.
void Application::RecordReturnToTheMenu()
{
    Ios::IosServices& services = console.GetServices();
    SystemMenu::IosNandFiles nandFiles(services.GetFileSystem(), services.GetTitles());
    SystemMenu::RecordMenuLeft(nandFiles, Formats::StateTypeReturnToMenu);
}

void Application::ForgetReportedProblems()
{
    hasReportedFatalError = false;
    reportedUnbuiltScreen.reset();
}

void Application::OnHostCommand(Platform::HostCommand command)
{
    switch (command)
    {
    case Platform::HostCommand::Power:
        console.GetServices().GetPower().ReportButtonPress(Ios::PowerEvent::PowerButtonPressed);
        return;

    case Platform::HostCommand::Reset:
        console.GetServices().GetPower().ReportButtonPress(Ios::PowerEvent::ResetButtonPressed);
        return;

    case Platform::HostCommand::ToggleSdCard:
        ToggleSdCard();
        return;
    }
}

void Application::ToggleSdCard()
{
    Ios::SdCardService& sdCard = console.GetServices().GetSdCard();
    if (sdCard.GetStatus().isInserted)
    {
        sdCard.RemoveCard();
        return;
    }

    constexpr bool isWriteProtected = false;
    Ios::Result<void> inserted = sdCard.InsertCard(folders.sdCardFolder, isWriteProtected);
    if (inserted.Failed())
        std::cout << std::format("There is no SD card: the folder {} is not there.\n", folders.sdCardFolder.string());
}

// The program is the only place that can tell the person who runs it: the menu has no screen for a screen that is not built, and it
// ends with the error screen when one of its files fails.
void Application::ReportProblems()
{
    if (session->Menu() == nullptr)
        return;

    SystemMenu::MenuSystem& menu = *session->Menu();

    const std::optional<std::string>& fatalErrorMessage = menu.FatalErrorMessage();
    if (fatalErrorMessage && !hasReportedFatalError)
    {
        hasReportedFatalError = true;
        std::cout << "The menu stopped: " << *fatalErrorMessage << "\n";
    }

    std::optional<SystemMenu::ScreenId> unbuiltScreen = menu.Services().screens.UnbuiltScreen();
    if (!unbuiltScreen || unbuiltScreen == reportedUnbuiltScreen)
        return;

    reportedUnbuiltScreen = unbuiltScreen;
    std::cout << std::format("The screen {:#04x} is not built yet. The picture stays black. Close the window to end the program.\n",
        static_cast<int>(*unbuiltScreen));
}

bool Application::IsPoweredOff() const
{
    return isPoweredOff;
}

Platform::Window& Application::GetWindow()
{
    return window;
}

Graphics::VulkanGraphicsDevice& Application::GetDevice()
{
    return device;
}

SystemMenu::MenuSystem& Application::GetMenu()
{
    if (session->Menu() == nullptr)
        throw std::logic_error("The menu was asked for while a channel runs");

    return *session->Menu();
}

Channels::ChannelSystem* Application::GetChannel()
{
    return session->Channel();
}

Ios::TitleId Application::RunningTitle() const
{
    return console.GetBootInformation().titleId;
}

ProgramSound& Application::GetSound()
{
    return sound;
}

Input::VirtualRemote& Application::GetVirtualRemote()
{
    return session->VirtualRemote();
}

}
