#include "WeeOS/Application.h"

#include "Channels/ChannelSystem.h"
#include "Channels/HomeMenuWaveSounds.h"
#include "Channels/NativeChannels.h"
#include "Input/SharedRemote.h"
#include "Input/VirtualRemote.h"
#include "Ios/Keyboard/PcKeyboard.h"
#include "Platform/FilePicker.h"
#include "Platform/MessagePump.h"
#include "Settings/SettingsError.h"
#include "SystemMenu/BackgroundServices/StartServices.h"
#include "SystemMenu/DataManagement/DataManagementServices.h"
#include "SystemMenu/DataManagement/MenuCertificates.h"
#include "SystemMenu/InstalledTitleContents.h"
#include "SystemMenu/IosNandFiles.h"
#include "SystemMenu/MenuSound.h"
#include "SystemMenu/MenuStateFile.h"
#include "SystemMenu/MessageBoard/MessageBoardData.h"
#include "SystemMenu/MessageBoard/PlayLog.h"
#include "SystemMenu/OwnTitleContents.h"
#include "SystemMenu/ScreenManager.h"
#include "SystemMenu/WiiMenu/IosMenuConsole.h"
#include "WeeOS/NewConsole.h"
#include "WeeOS/WiiSettingsServices.h"

#include <chrono>
#include <format>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace WeeOS
{

namespace
{

constexpr std::wstring_view WindowTitle = L"WeeOS";
constexpr std::wstring_view NandPickerTitle = L"Import your Wii: choose nand.bin of its BootMii backup";
constexpr std::wstring_view NandImageFileName = L"nand.bin";
constexpr std::string_view NoBackupChosenMessage =
    "WeeOS runs the system of your own Wii. Choose nand.bin of its BootMii backup (keys.bin beside it) at the first start.";

// One step of the console, 1001/60000 of a second, in whole microseconds.
constexpr std::chrono::microseconds StepOfAHiddenWindow(16683);
constexpr double SecondsOfAStep = 1001.0 / 60000.0;

SystemMenu::ClockFormat ClockFormatOf(const Platform::RegionalSettings& regionalSettings)
{
    SystemMenu::ClockFormat format;
    format.usesTwentyFourHours = regionalSettings.usesTwentyFourHourClock;
    format.showsLeadingZeroOfHours = regionalSettings.showsLeadingZeroOfHours;
    return format;
}

// The browser engine of the Internet Channel keeps its cache and its cookies in this folder beside the NAND folder.
constexpr std::string_view BrowserFolderName = "Browser";

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

Input::SensorBarPosition SensorBarPositionOf(Settings::SensorBarPosition position)
{
    if (position == Settings::SensorBarPosition::AboveTelevision)
        return Input::SensorBarPosition::AboveScreen;

    return Input::SensorBarPosition::BelowScreen;
}

}

std::optional<std::filesystem::path> PickNandWithWindows()
{
    return Platform::PickFile(std::wstring(NandPickerTitle), std::wstring(NandImageFileName));
}

// The program of the title that runs, the menu or a channel, with everything it reads through the services of the IOS that runs, and
// the keys of the PC that go to those services.
// The window sends its events to the keyboard service, and the keyboard service to the virtual remote.
// Remote 1 is the virtual remote shared with the first real controller; the real controllers on the other channels are plugged into
// the remote service of this IOS in every step.
class Application::Session
{
public:
    Session(Ios::IosServices& iosServices, Ios::TitleId runningTitle, Platform::Window& programWindow,
        Graphics::GraphicsDevice& graphicsDevice, Settings::PcSettingsStore& pcSettings, ProgramSound& programSound,
        const HostEnvironment& host, SystemMenu::ConsoleImport& importOfAWii, const std::filesystem::path& browserEngineFolder,
        Input::RealControllers& controllers)
        : window(programWindow)
        , consoleImport(importOfAWii)
        , browserFolder(browserEngineFolder)
        , sound(programSound)
        , titleContents(iosServices.GetTitles())
        , systemMenuContents(iosServices.GetTitles(), Ios::SystemMenuTitleId)
        , nandFiles(iosServices.GetFileSystem(), iosServices.GetTitles())
        , wiiSettings(iosServices, programWindow, programSound, host.regionalSettings, pcSettings, controllers)
        , wiiMenuConsole(iosServices, host.clock, ClockFormatOf(host.regionalSettings))
        , keyboard(iosServices.GetKeyboard(), iosServices.GetRemote().GetVirtualRemote())
        , virtualRemote(iosServices.GetRemote().GetVirtualRemote())
        , remotes(iosServices.GetRemote())
        , realControllers(controllers)
        , remoteOne(virtualRemote)
    {
        remotes.AttachSource(Ios::VirtualRemoteChannel, remoteOne);
        PlugInControllers();

        if (runningTitle == Ios::SystemMenuTitleId)
            StartMenu(iosServices, graphicsDevice);
        else
            StartChannel(iosServices, graphicsDevice, runningTitle, host.clock);

        window.SetListener(&keyboard);
    }

    ~Session()
    {
        sound.StopAllSounds();
        sound.StopTitleArchives();
        window.SetListener(nullptr);
        UnplugControllers();
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
        PlugInControllers();

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

    void AskToTurnOff()
    {
        if (menu)
            menu->AskToTurnOff();
        else
            channel->AskToTurnOff();
    }

private:
    // The real controllers as they are after their update, and the settings of the console that they need: the sensor bar position
    // (BT.BAR) in every step, as the menu gives it to the pointer library, and the camera sensitivity (BT.SENS).
    void PlugInControllers()
    {
        remoteOne.SetRealSource(realControllers.SourceOfChannel(Ios::VirtualRemoteChannel));

        for (int channelNumber = Ios::VirtualRemoteChannel + 1; channelNumber < Input::RemoteChannelCount; ++channelNumber)
        {
            Input::RemoteSource* source = realControllers.SourceOfChannel(channelNumber);
            if (source == pluggedSources[channelNumber])
                continue;

            if (source != nullptr)
                remotes.AttachSource(channelNumber, *source);
            else
                remotes.DetachSource(channelNumber);
            pluggedSources[channelNumber] = source;
        }

        remotes.SetSensorBarPosition(SensorBarPositionOf(wiiSettings.SensorBarPosition()));
        realControllers.SetCameraSensitivity(wiiSettings.PointerSensitivity());
    }

    // The service keeps the virtual remote alone on channel 0 again, so that nothing of this session stays plugged in.
    void UnplugControllers()
    {
        for (int channelNumber = Ios::VirtualRemoteChannel + 1; channelNumber < Input::RemoteChannelCount; ++channelNumber)
        {
            if (pluggedSources[channelNumber] != nullptr)
                remotes.DetachSource(channelNumber);
        }

        remotes.AttachSource(Ios::VirtualRemoteChannel, virtualRemote);
    }

    void StartMenu(Ios::IosServices& iosServices, Graphics::GraphicsDevice& graphicsDevice)
    {
        wiiSettings.FlowServices().consoleImport = &consoleImport;
        menu = std::make_unique<SystemMenu::MenuSystem>(graphicsDevice, titleContents, nandFiles, iosServices.GetRemote(),
            iosServices.GetPower(), wiiMenuConsole, &wiiSettings.FlowServices(), &consoleImport);
        SystemMenu::RunStartServices(iosServices.GetFileSystem(), iosServices.GetTitles());
        menu->UsePhysicalKeyboard(iosServices.GetKeyboard());
        Ios::TitleService& titles = iosServices.GetTitles();
        menu->Services().titleLaunch.SetProgramCheck([&titles](Ios::TitleId titleId) { return CanStart(titles, titleId); });

        SystemMenu::DataManagementServices dataManagementServices{iosServices.GetTitles(), iosServices.GetFileSystem(),
            iosServices.GetSdCard(), iosServices.GetConsoleIdentity(), SystemMenu::ReadMenuCertificates(menu->Services().program)};
        dataManagement = std::make_unique<SystemMenu::DataManagementServices>(std::move(dataManagementServices));
        menu->Services().dataManagement = dataManagement.get();

        Ios::MacAddress wirelessAddress = iosServices.GetNetworkConfig().GetWirelessMacAddress();
        messageBoard = std::make_unique<SystemMenu::MessageBoardData>(nandFiles, wirelessAddress, graphicsDevice, menu->Services().files);
        menu->Services().messageBoard = messageBoard.get();
        SystemMenu::NandFileAccess menuFiles(iosServices.GetFileSystem(), iosServices.GetTitles());
        SystemMenu::PutTheLastPlayOnTheBoard(menuFiles, messageBoard->store, menu->Services().messages, wiiMenuConsole.GetLocalTime());

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

    void StartChannel(Ios::IosServices& iosServices, Graphics::GraphicsDevice& graphicsDevice, Ios::TitleId runningTitle,
        const Settings::PcClock& consoleTime)
    {
        Channels::ChannelStorage storage{iosServices.GetSdCard(), iosServices.GetFileSystem(), iosServices.GetTitles().GetCaller()};
        std::string serverAddress = wiiSettings.FlowServices().pcSettings.Values().serverAddress;
        Channels::ChannelNetwork network{iosServices.GetNetworkConfig(), iosServices.GetSockets(), serverAddress};
        Channels::ChannelAudio audio{iosServices.GetMicrophone(), sound.Waves()};
        Channels::ChannelPc pc{&iosServices.GetKeyboard(), {browserFolder, sound.IsSilent()}};
        channel = std::make_unique<Channels::ChannelSystem>(graphicsDevice, titleContents, systemMenuContents, iosServices.GetRemote(),
            network, storage, audio, iosServices.GetPower(), runningTitle, consoleTime,
            Channels::ReadConsoleFontArchive(iosServices.GetTitles()), pc);

        std::vector<Sound::SoundSystem*> channelSounds;
        for (const Formats::Bytes& archive : Channels::ReadSoundArchives(runningTitle, titleContents))
        {
            Sound::SoundSystem* soundSystem = sound.StartTitleArchive(archive);
            if (soundSystem != nullptr)
                channelSounds.push_back(soundSystem);
        }

        if (!channelSounds.empty())
            channelSound.emplace(channelSounds);
        channelWaveSounds = Channels::RenderHomeMenuSounds(runningTitle, titleContents);
    }

    // A channel asks for its sounds by name, as the menu does; each one plays from the first of the channel's archives that has it.
    void PlayChannelSoundsOfTheStep()
    {
        sound.SetSoundMode(wiiSettings.SoundMode());
        if (channelSound)
            channelSound->PlayRequests(channel->Services().sounds);
        PlayWaveSoundsOfTheStep();
    }

    // The sounds that the channel keeps as MIDI files (the Internet Channel's HOME menu) play as the waves rendered at its start.
    void PlayWaveSoundsOfTheStep()
    {
        for (const SystemMenu::SoundRequest& request : channel->Services().sounds.AllRequestsInThisStep())
        {
            if (request.action != SystemMenu::SoundAction::PlayEffect)
                continue;

            auto rendered = channelWaveSounds.find(request.effectName);
            if (rendered != channelWaveSounds.end())
                sound.Waves().Play(rendered->second);
        }
    }

    Platform::Window& window;
    SystemMenu::ConsoleImport& consoleImport;
    std::filesystem::path browserFolder;
    ProgramSound& sound;
    SystemMenu::OwnTitleContents titleContents;
    SystemMenu::InstalledTitleContents systemMenuContents;
    SystemMenu::IosNandFiles nandFiles;
    WiiSettingsServices wiiSettings;
    SystemMenu::IosMenuConsole wiiMenuConsole;

    // Declared before the menu, which keeps a pointer to it, so that the menu goes first.
    std::unique_ptr<SystemMenu::DataManagementServices> dataManagement;

    // The board keeps the faces of its Miis on the graphics device; it reads the files of the menu only while the menu is there.
    std::unique_ptr<SystemMenu::MessageBoardData> messageBoard;
    std::unique_ptr<SystemMenu::MenuSystem> menu;
    std::unique_ptr<Channels::ChannelSystem> channel;
    Ios::PcKeyboard keyboard;
    Input::VirtualRemote& virtualRemote;
    Ios::RemoteService& remotes;
    Input::RealControllers& realControllers;
    Input::SharedRemote remoteOne;
    std::array<Input::RemoteSource*, Input::RemoteChannelCount> pluggedSources = {};
    std::optional<SystemMenu::MenuSound> menuSound;
    std::optional<SystemMenu::MenuSound> channelSound;
    Channels::RenderedSounds channelWaveSounds;
};

Application::Application(const Ios::IosFolders& iosFolders, bool isSilent, HostEnvironment hostEnvironment)
    : folders(iosFolders)
    , host(std::move(hostEnvironment))
    , importer(iosFolders, host.pickNand)
    , console(iosFolders, host.isPcConnected)
    , pcSettings(host.pcSettingsFile)
    , controllers(*host.inputDevices)
    , window(WindowTitle, Platform::DefaultClientSize, host.windowVisibility)
    , device(window)
    , sound(isSilent)
    , pacer(timer.ElapsedSeconds())
{
    PlaceWindow();
    PrepareConsoleFolders();
    console.PowerOn();
    StartSession();
}

// The SD card is always in its slot: its folder is made when it is not there. A NAND folder without a console gets one: a new one
// from the Wii files beside WeeOS.exe, whose first start offers the import of the person's own Wii, or, without Wii files (the public
// download carries none), the person's own Wii at once.
void Application::PrepareConsoleFolders()
{
    if (!folders.sdCardFolder.empty())
        std::filesystem::create_directories(folders.sdCardFolder);

    if (HasConsole(folders))
        return;

    if (!std::filesystem::exists(host.wiiFilesFolder))
    {
        ImportTheOwnWii();
        return;
    }

    std::cout << "This is the first start: WeeOS makes a new console.\n";
    MakeNewConsole(folders, host.wiiFilesFolder);
    importer.OfferAtStart();
}

void Application::ImportTheOwnWii()
{
    std::cout << "This is the first start: WeeOS imports your Wii.\n";
    std::optional<std::filesystem::path> nandImage = host.pickNand();
    if (!nandImage)
        throw std::runtime_error(std::string(NoBackupChosenMessage));

    ImportConsole(*nandImage, folders);
}

Application::~Application() = default;

void Application::StartSession()
{
    std::filesystem::path browserFolder = folders.nandFolder.parent_path() / BrowserFolderName;
    session = std::make_unique<Session>(console.GetServices(), RunningTitle(), window, device, pcSettings, sound, host, importer,
        browserFolder, controllers);
    AttachMicrophone();
    StopOnTheKeyOfKoreanConsoles();
}

// Fullscreen last, so that the window leaves fullscreen to its own place and size.
void Application::PlaceWindow()
{
    const Settings::PcSettings& settings = pcSettings.Values();

    Platform::WindowPlace place = window.WindowedPlace();
    place.clientSize = {settings.windowWidth, settings.windowHeight};
    if (settings.windowLeft && settings.windowTop)
    {
        place.left = *settings.windowLeft;
        place.top = *settings.windowTop;
    }
    window.SetWindowedPlace(place);

    if (settings.isFullscreen)
        window.ToggleFullscreen();
}

// IOS is new in every session, so the accessory is plugged into the new microphone service. A channel looks for it in its first step.
void Application::AttachMicrophone()
{
    Ios::MicrophoneService& microphone = console.GetServices().GetMicrophone();
    if (givenMicrophone != nullptr)
    {
        microphone.AttachSource(*givenMicrophone);
        return;
    }

    if (Sound::HasCaptureDevice())
        microphone.AttachSource(pcMicrophone);
}

void Application::UseMicrophoneSource(Ios::MicrophoneSource& source)
{
    givenMicrophone = &source;
    console.GetServices().GetMicrophone().AttachSource(source);
}

const PcMicrophone& Application::GetPcMicrophone() const
{
    return pcMicrophone;
}

// The System object of the original asks the title service this when the menu starts (0x8133400C). A failed question is no error.
void Application::StopOnTheKeyOfKoreanConsoles()
{
    if (session->Menu() == nullptr)
        return;

    Ios::Result<bool> hasKoreanKey = console.GetServices().GetTitles().CheckHasKoreanKey();
    if (hasKoreanKey.Succeeded() && hasKoreanKey.Value())
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
    HandleTheWindow();

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
    WaitForTheNextStepOfAHiddenWindow();
}

void Application::RunOneStep()
{
    Platform::ProcessPendingMessages();
    HandleTheWindow();

    if (isPoweredOff)
        return;

    session->VirtualRemote().SetWindowSize(window.ClientSize());
    RunStep();
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

void Application::HandleTheWindow()
{
    AskBeforeClosing();
    RememberWindowPlace();
}

// A minimized window cannot show the question, nor the picture that fades out, so it powers off at once.
void Application::AskBeforeClosing()
{
    if (!window.TakeCloseRequest())
        return;

    if (window.IsMinimized())
        isPoweredOff = true;
    else
        session->AskToTurnOff();
}

// The settings of the PC remember where the user put the window and whether it is fullscreen, for the next start.
void Application::RememberWindowPlace()
{
    if (!window.TakePlaceChange())
        return;

    Platform::WindowPlace place = window.WindowedPlace();
    Settings::PcSettings settings = pcSettings.Values();
    settings.windowLeft = place.left;
    settings.windowTop = place.top;
    settings.windowWidth = place.clientSize.width;
    settings.windowHeight = place.clientSize.height;
    settings.isFullscreen = window.IsFullscreen();

    try
    {
        pcSettings.Store(settings);
    }
    catch (const Settings::SettingsError& error)
    {
        std::cout << "The place of the window cannot be remembered: " << error.what() << "\n";
    }
}

// The display of a shown window paces the pictures (Present waits for its refresh). A hidden window has none, so the pictures come
// one step apart instead, and a hand that arrives and a button that is pressed in two pictures reach the menu in two steps.
void Application::WaitForTheNextStepOfAHiddenWindow()
{
    if (window.IsShown())
        return;

    std::this_thread::sleep_for(StepOfAHiddenWindow);
}

void Application::RunDueSteps()
{
    session->VirtualRemote().SetWindowSize(window.ClientSize());

    int dueSteps = pacer.TakeDueSteps(timer.ElapsedSeconds());
    for (int step = 0; step < dueSteps; ++step)
        RunStep();
}

// The controllers count the steps as their time, so that they go with the console also when a test runs the steps faster.
void Application::RunStep()
{
    controllerSeconds += SecondsOfAStep;
    controllers.Update(controllerSeconds);
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
    bool hasProgram = titleId == Ios::SystemMenuTitleId || Channels::HasNativeProgram(titleId);
    if (!hasProgram)
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

void Application::PressPowerButton()
{
    console.GetServices().GetPower().ReportButtonPress(Ios::PowerEvent::PowerButtonPressed);
}

void Application::PressResetButton()
{
    console.GetServices().GetPower().ReportButtonPress(Ios::PowerEvent::ResetButtonPressed);
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

Ios::IosServices& Application::GetServices()
{
    return console.GetServices();
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
