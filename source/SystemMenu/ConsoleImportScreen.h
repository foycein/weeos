#pragma once

#include "SystemMenu/MenuServices.h"
#include "SystemMenu/Screen.h"

#include <string>

namespace SystemMenu
{

// WeeOS's own first step on the first start of a new console, before the welcome (docs/DECISIONS.md, Start): a window of the menu asks
// whether to import the person's own Wii or to skip. Import lets the person pick the NAND of the Wii and shows that the console works
// while it is imported; the program then starts the imported Wii. Skip, and a failed import that the person confirms and skips,
// fade out to Health and Safety, the welcome screen.
class ConsoleImportScreen : public Screen
{
public:
    explicit ConsoleImportScreen(MenuServices& menuServices);

protected:
    bool IsReady() override;
    bool RunMain() override;
    bool RunClosing() override;

private:
    enum class State
    {
        Asking,
        Importing,
        ClosingWaitWindow,
        ShowingFailure,
    };

    void Ask();
    void ShowThatTheConsoleWorks();
    void ShowFailure(const std::string& failure);
    bool RunAsking();
    void RunImporting();
    void RunClosingWaitWindow();
    void RunShowingFailure();

    MenuServices& services;
    State state = State::Asking;
    bool hasFadeOutStarted = false;
    std::string pendingFailure;
};

}
