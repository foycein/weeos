#include "Browser/PageThread.h"

#include "Browser/BrowserError.h"
#include "Browser/PageEngine.h"

#include <windows.h>

#include <future>

namespace Browser
{

namespace
{

// A message to the thread itself: commands are waiting.
constexpr UINT WakeMessage = WM_APP + 1;

void MakeMessageQueue()
{
    MSG message = {};
    PeekMessageW(&message, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
}

}

PageThread::PageThread(const BrowserSettings& settings)
{
    std::promise<unsigned long> queueIsReady;
    std::future<unsigned long> threadNumberOnceReady = queueIsReady.get_future();

    thread = std::thread([this, settings, ready = std::move(queueIsReady)]() mutable {
        MakeMessageQueue();
        ready.set_value(GetCurrentThreadId());
        Run(settings);
    });

    threadNumber = threadNumberOnceReady.get();
}

PageThread::~PageThread()
{
    PostThreadMessageW(threadNumber, WM_QUIT, 0, 0);
    thread.join();
}

void PageThread::Send(PageCommand command)
{
    {
        std::lock_guard<std::mutex> guard(commandLock);
        waitingCommands.push_back(std::move(command));
    }
    Wake();
}

PageReport& PageThread::Report()
{
    return report;
}

void PageThread::Run(BrowserSettings settings)
{
    HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    // The window of the page is measured in real pixels of the screen, also in a program that Windows would otherwise scale.
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    try
    {
        PageEngine engine(settings, report, [this]() { Wake(); });
        RunMessageLoop(engine);
    }
    catch (const BrowserError& error)
    {
        std::lock_guard<std::mutex> guard(report.lock);
        report.startError = error.what();
    }

    if (SUCCEEDED(comResult))
        CoUninitialize();
}

void PageThread::RunMessageLoop(PageEngine& engine)
{
    MSG message = {};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        if (message.hwnd == nullptr && message.message == WakeMessage)
        {
            RunWaitingCommands(engine);
            continue;
        }

        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

void PageThread::RunWaitingCommands(PageEngine& engine)
{
    if (!engine.IsReady())
        return;

    std::vector<PageCommand> commands;
    {
        std::lock_guard<std::mutex> guard(commandLock);
        commands.swap(waitingCommands);
    }

    for (PageCommand& command : commands)
        command(engine);
}

void PageThread::Wake()
{
    PostThreadMessageW(threadNumber, WakeMessage, 0, 0);
}

}
