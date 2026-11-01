#pragma once

#include "Browser/PageReport.h"
#include "Browser/WebPage.h"

#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace Browser
{

class PageEngine;

// Something for the engine to do, sent from the program's thread.
using PageCommand = std::function<void(PageEngine& engine)>;

// The thread of the browser engine. WebView2 needs a thread whose COM is single-threaded and that runs a message loop; the program's
// own thread has COM for the sound already (multithreaded), so the engine gets a thread of its own. Commands wait until the engine is ready.
class PageThread
{
public:
    explicit PageThread(const BrowserSettings& settings);
    ~PageThread();

    PageThread(const PageThread&) = delete;
    PageThread& operator=(const PageThread&) = delete;

    void Send(PageCommand command);
    PageReport& Report();

private:
    void Run(BrowserSettings settings);
    void RunMessageLoop(PageEngine& engine);
    void RunWaitingCommands(PageEngine& engine);
    void Wake();

    PageReport report;
    std::mutex commandLock;
    std::vector<PageCommand> waitingCommands;
    std::thread thread;

    // The thread number of Windows, known once the thread has a message queue.
    unsigned long threadNumber = 0;
};

}
