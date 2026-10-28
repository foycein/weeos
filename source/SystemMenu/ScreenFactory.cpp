#include "SystemMenu/ScreenFactory.h"

#include "SystemMenu/HealthAndSafetyScreen.h"

namespace SystemMenu
{

std::unique_ptr<Screen> MakeScreen(ScreenId screen, [[maybe_unused]] int argument, MenuServices& services)
{
    if (screen == ScreenId::HealthAndSafety)
        return std::make_unique<HealthAndSafetyScreen>(services);

    return nullptr;
}

}
