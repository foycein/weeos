#include "SystemMenu/ScreenFactory.h"

#include "SystemMenu/HealthAndSafetyScreen.h"
#include "SystemMenu/WiiSettings/SettingsScreen.h"

namespace SystemMenu
{

std::unique_ptr<Screen> MakeScreen(ScreenId screen, int argument, MenuServices& services)
{
    if (screen == ScreenId::HealthAndSafety)
        return std::make_unique<HealthAndSafetyScreen>(services);

    if (screen == ScreenId::WiiSettings)
        return std::make_unique<SettingsScreen>(services, argument);

    return nullptr;
}

}
