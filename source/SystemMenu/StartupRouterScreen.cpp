#include "SystemMenu/StartupRouterScreen.h"

#include "SystemMenu/Fader.h"
#include "SystemMenu/FirstScreen.h"
#include "SystemMenu/HomeMenu/HomeButton.h"
#include "SystemMenu/ScreenManager.h"
#include "SystemMenu/SoundRequests.h"

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
    services.homeButton.Start();
    if (argument == RouterToSettingsCountryPage)
        services.screens.ReplaceScreen(*this, ScreenId::WiiSettings, SettingsCountryPage);

    if (argument == RouterToWiiOptions)
        services.screens.ReplaceScreen(*this, ScreenId::WiiOptionsBackground, WiiOptionsChoices);

    services.sounds.StartMusic();
    services.fader.StartFadeIn();
    return true;
}

}
