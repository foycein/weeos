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

}

Application::Application(const Ios::IosFolders& folders)
    : console(folders)
    , window(WindowTitle)
    , keyboard(console.Services().GetKeyboard(), console.Services().GetRemote().GetVirtualRemote())
    , device(window)
    , titleContents(console.Services().GetTitles())
    , wiiSettings(console.Services(), window, GetVirtualRemote())
    , menu(device, titleContents, console.Services().GetRemote(), wiiSettings.PictureShape(), &wiiSettings.FlowServices())
    , pacer(timer.ElapsedSeconds())
{
    wiiSettings.UseDisplay(menu.Services().display);
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
    return console.Services().GetRemote().GetVirtualRemote();
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
