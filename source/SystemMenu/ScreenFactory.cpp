#include "SystemMenu/ScreenFactory.h"

#include "SystemMenu/BackToWiiMenuScreen.h"
#include "SystemMenu/HealthAndSafetyScreen.h"
#include "SystemMenu/LimitOverScreen.h"
#include "SystemMenu/ParentalDialogScreen.h"
#include "SystemMenu/StartupRouterScreen.h"

namespace SystemMenu
{

std::unique_ptr<Screen> MakeScreen(ScreenId screen, int argument, MenuServices& services)
{
    switch (screen)
    {
    case ScreenId::HealthAndSafety:
        return std::make_unique<HealthAndSafetyScreen>(services);

    case ScreenId::BackToWiiMenu:
        return std::make_unique<BackToWiiMenuScreen>(services);

    case ScreenId::LimitOver:
        return std::make_unique<LimitOverScreen>(services);

    case ScreenId::ParentalDialog:
        return std::make_unique<ParentalDialogScreen>(services, argument);

    case ScreenId::StartupRouter:
        return std::make_unique<StartupRouterScreen>(services, argument);

    default:
        return nullptr;
    }
}

}
