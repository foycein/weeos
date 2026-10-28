#include "WeeOS/Application.h"

#include "Input/VirtualRemote.h"
#include "Ios/Keyboard/PcKeyboard.h"
#include "Platform/MessagePump.h"
#include "SystemMenu/IosNandFiles.h"
#include "SystemMenu/KeyboardDigitEntry.h"
#include "SystemMenu/OwnTitleContents.h"
#include "SystemMenu/ScreenManager.h"

#include <format>
#include <iostream>

namespace WeeOS
{

namespace
{

constexpr std::wstring_view WindowTitle = L"WeeOS";

// The picture of the program has the shape of a 16:9 screen. The mouse points into this picture.
constexpr Platform::ScreenShape PictureShape = Platform::ScreenShape::SixteenByNine;

}

// The menu with everything it reads through the services of the IOS that runs, and the keys of the PC that go to those services.
// The window sends its events to the keys, the keys to the keyboard service, and the keyboard service to the virtual remote.
class Application::Session
{
public:
    Session(Ios::IosServices& iosServices, Platform::Window& programWindow, Graphics::GraphicsDevice& graphicsDevice,
        Platform::HostCommandListener& commandListener)
        : window(programWindow)
        , titleContents(iosServices.GetTitles())
        , nandFiles(iosServices.GetFileSystem(), iosServices.GetTitles())
        , textEntry(iosServices.GetKeyboard())
        , menu(graphicsDevice, titleContents, nandFiles, iosServices.GetRemote(), iosServices.GetPower(), textEntry, PictureShape)
        , keyboard(iosServices.GetKeyboard(), iosServices.GetRemote().GetVirtualRemote())
        , hostKeys(keyboard, commandListener)
        , virtualRemote(iosServices.GetRemote().GetVirtualRemote())
    {
        virtualRemote.SetScreenShape(PictureShape);
        window.SetListener(&hostKeys);
    }

    ~Session()
    {
        window.SetListener(nullptr);
    }

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    SystemMenu::MenuSystem& Menu()
    {
        return menu;
    }

    Input::VirtualRemote& VirtualRemote()
    {
        return virtualRemote;
    }

private:
    Platform::Window& window;
    SystemMenu::OwnTitleContents titleContents;
    SystemMenu::IosNandFiles nandFiles;
    SystemMenu::KeyboardDigitEntry textEntry;
    SystemMenu::MenuSystem menu;
    Ios::PcKeyboard keyboard;
    Platform::HostKeys hostKeys;
    Input::VirtualRemote& virtualRemote;
};

Application::Application(const Ios::IosFolders& iosFolders)
    : folders(iosFolders)
    , console(iosFolders)
    , window(WindowTitle)
    , device(window)
    , pacer(timer.ElapsedSeconds())
{
    console.PowerOn();
    StartSession();
}

Application::~Application() = default;

void Application::StartSession()
{
    Platform::HostCommandListener& commandListener = *this;
    session = std::make_unique<Session>(console.GetServices(), window, device, commandListener);
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
    CarryOutPowerRequests();
    ReportProblems();

    if (isPoweredOff)
        return;

    session->Menu().Draw();
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
        session->Menu().Update();
}

void Application::CarryOutPowerRequests()
{
    Ios::EventQueue<Ios::PowerRequest>& requests = console.GetServices().GetPower().Requests();

    std::optional<Ios::PowerRequest> request = requests.TryTake();
    if (!request)
        return;

    if (*request == Ios::PowerRequest::Shutdown)
        isPoweredOff = true;
    else
        ResetConsole();
}

// IOS starts again, so everything that holds a service goes first and the new session takes the new services.
void Application::ResetConsole()
{
    session.reset();
    console.PowerOff();
    console.PowerOn();
    StartSession();
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
    SystemMenu::MenuSystem& menu = session->Menu();

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
    return session->Menu();
}

}
