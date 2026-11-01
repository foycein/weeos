#include "SystemMenu/ScreenFactory.h"

#include "SystemMenu/HealthAndSafetyScreen.h"
#include "SystemMenu/WiiOptionsBackgroundScreen.h"
#include "SystemMenu/WiiOptionsBarScreen.h"
#include "SystemMenu/WiiOptionsChoicesScreen.h"
#include "SystemMenu/WiiMenu/BottomBarScreen.h"
#include "SystemMenu/WiiMenu/ChannelGridScreen.h"
#include "SystemMenu/WiiMenu/MainFrameScreen.h"

namespace SystemMenu
{

std::unique_ptr<Screen> MakeScreen(ScreenId screen, int argument, MenuServices& services)
{
    if (screen == ScreenId::HealthAndSafety)
        return std::make_unique<HealthAndSafetyScreen>(services);

    if (screen == ScreenId::MainFrame)
        return std::make_unique<MainFrameScreen>(services);

    if (screen == ScreenId::BottomBar)
        return std::make_unique<BottomBarScreen>(services);

    if (screen == ScreenId::BarOverlay)
        return std::make_unique<BarOverlayScreen>(services);

    if (screen == ScreenId::ChannelGrid)
        return std::make_unique<ChannelGridScreen>(services, argument);

    if (screen == ScreenId::WiiOptionsBackground)
        return std::make_unique<WiiOptionsBackgroundScreen>(services, argument);

    if (screen == ScreenId::WiiOptionsBar)
        return std::make_unique<WiiOptionsBarScreen>(services);

    if (screen == ScreenId::WiiOptionsChoices)
        return std::make_unique<WiiOptionsChoicesScreen>(services, argument);

    return nullptr;
}

}
