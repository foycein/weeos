#include "SystemMenu/ScreenFactory.h"

#include "SystemMenu/HealthAndSafetyScreen.h"
#include "SystemMenu/WiiOptionsBackgroundScreen.h"
#include "SystemMenu/WiiOptionsBarScreen.h"
#include "SystemMenu/WiiOptionsChoicesScreen.h"
#include "SystemMenu/WiiSettings/SettingsScreen.h"

namespace SystemMenu
{

std::unique_ptr<Screen> MakeScreen(ScreenId screen, int argument, MenuServices& services)
{
    if (screen == ScreenId::HealthAndSafety)
        return std::make_unique<HealthAndSafetyScreen>(services);

    if (screen == ScreenId::WiiOptionsBackground)
        return std::make_unique<WiiOptionsBackgroundScreen>(services, argument);

    if (screen == ScreenId::WiiOptionsBar)
        return std::make_unique<WiiOptionsBarScreen>(services);

    if (screen == ScreenId::WiiOptionsChoices)
        return std::make_unique<WiiOptionsChoicesScreen>(services, argument);

    // A menu that runs without the settings of the console (the tests of the other screens) has no Wii Settings.
    if (screen == ScreenId::WiiSettings && services.wiiSettings != nullptr)
        return std::make_unique<SettingsScreen>(services, argument);

    return nullptr;
}

}
