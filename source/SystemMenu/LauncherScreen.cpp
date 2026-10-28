#include "SystemMenu/LauncherScreen.h"

#include "SystemMenu/ScreenManager.h"

namespace SystemMenu
{

LauncherScreen::LauncherScreen(ScreenManager& screenManager, ScreenId firstScreenToOpen, int argumentOfFirstScreen)
    : Screen(ScreenId::Launcher)
    , screens(screenManager)
    , firstScreen(firstScreenToOpen)
    , firstScreenArgument(argumentOfFirstScreen)
{
}

void LauncherScreen::Build()
{
    screens.OpenChild(firstScreen, Id(), firstScreenArgument);
}

}
