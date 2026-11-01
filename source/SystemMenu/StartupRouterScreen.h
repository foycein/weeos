#pragma once

#include "SystemMenu/MenuServices.h"
#include "SystemMenu/Screen.h"

namespace SystemMenu
{

// The screen that routes a start with a request (screen 0x20). It has no picture. It makes the picture fade in and replaces itself
// by the screen the request asks for: a page of the settings, or Wii Options. Its argument says which (see FirstScreen.h).
class StartupRouterScreen : public Screen
{
public:
    StartupRouterScreen(MenuServices& menuServices, int routerArgument);

protected:
    // The original waits here until the start-up of the menu has run. WeeOS has no such wait.
    bool RunMain() override;

private:
    MenuServices& services;
    int argument;
};

}
