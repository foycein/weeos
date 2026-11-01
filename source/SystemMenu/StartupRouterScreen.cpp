#include "SystemMenu/StartupRouterScreen.h"

#include "SystemMenu/Fader.h"
#include "SystemMenu/FirstScreen.h"
#include "SystemMenu/ScreenManager.h"

namespace SystemMenu
{

namespace
{

// The pages that the router opens: the page of the settings where the user chooses the country, and the choices of Wii Options.
constexpr int SettingsCountryPage = 6;
constexpr int WiiOptionsChoices = 2;

}

StartupRouterScreen::StartupRouterScreen(MenuServices& menuServices, int routerArgument)
    : Screen(ScreenId::StartupRouter)
    , services(menuServices)
    , argument(routerArgument)
{
}

bool StartupRouterScreen::RunMain()
{
    if (argument == RouterToSettingsCountryPage)
        services.screens.ReplaceScreen(*this, ScreenId::WiiSettings, SettingsCountryPage);

    if (argument == RouterToWiiOptions)
        services.screens.ReplaceScreen(*this, ScreenId::WiiOptionsBackground, WiiOptionsChoices);

    services.fader.StartFadeIn();
    return true;
}

}
