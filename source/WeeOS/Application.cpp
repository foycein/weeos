#include "WeeOS/Application.h"

#include "Platform/MessagePump.h"
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

Ios::IosServices& PowerOn(Boot::ConsoleStartup& console)
{
    console.PowerOn();
    return console.GetServices();
}

}

Application::Application(const Ios::IosFolders& folders)
    : console(folders)
    , services(PowerOn(console))
    , window(WindowTitle)
    , keyboard(services.GetKeyboard(), services.GetRemote().GetVirtualRemote())
    , device(window)
    , titleContents(services.GetTitles())
    , menu(device, titleContents, services.GetRemote(), PictureShape)
    , pacer(timer.ElapsedSeconds())
{
    GetVirtualRemote().SetScreenShape(PictureShape);
    window.SetListener(&keyboard);
}

Application::~Application()
{
    window.SetListener(nullptr);
}

void Application::Run()
{
    while (!IsCloseRequested())
        ShowNextPicture();
}

void Application::ShowNextPicture()
{
    Platform::ProcessPendingMessages();
    if (IsCloseRequested())
        return;

    if (window.IsMinimized())
    {
        Platform::WaitForMessage();
        return;
    }

    RunDueSteps();
    ReportUnbuiltScreen();
    menu.Draw();
    device.Present();
}

bool Application::IsCloseRequested() const
{
    return window.IsCloseRequested();
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
    return menu;
}

Input::VirtualRemote& Application::GetVirtualRemote()
{
    return services.GetRemote().GetVirtualRemote();
}

void Application::RunDueSteps()
{
    GetVirtualRemote().SetWindowSize(window.ClientSize());

    int dueSteps = pacer.TakeDueSteps(timer.ElapsedSeconds());
    for (int step = 0; step < dueSteps; ++step)
        menu.Update();
}

void Application::ReportUnbuiltScreen()
{
    std::optional<SystemMenu::ScreenId> unbuiltScreen = menu.Services().screens.UnbuiltScreen();
    if (!unbuiltScreen || unbuiltScreen == reportedUnbuiltScreen)
        return;

    reportedUnbuiltScreen = unbuiltScreen;
    std::cout << std::format("The screen {:#04x} is not built yet. The picture stays black. Close the window to end the program.\n",
        static_cast<int>(*unbuiltScreen));
}

}
