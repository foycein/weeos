#include "SystemMenu/ScreenFactory.h"

#include "SystemMenu/BackToWiiMenuScreen.h"
#include "SystemMenu/DataManagement/DataEditorScreen.h"
#include "SystemMenu/SdCardMenu/SdCardMenuScreens.h"
#include "SystemMenu/SdCardMenu/SdChannelWindowScreen.h"
#include "SystemMenu/LimitOverScreen.h"
#include "SystemMenu/StartupRouterScreen.h"
#include "SystemMenu/WelcomeScreen.h"
#include "SystemMenu/WiiOptionsBackgroundScreen.h"
#include "SystemMenu/WiiOptionsBarScreen.h"
#include "SystemMenu/WiiOptionsChoicesScreen.h"
#include "SystemMenu/WiiMenu/BottomBarScreen.h"
#include "SystemMenu/WiiMenu/ChannelGridScreen.h"
#include "SystemMenu/WiiMenu/ChannelStartWindowScreen.h"
#include "SystemMenu/WiiMenu/MainFrameScreen.h"
#include "SystemMenu/WiiSettings/SettingsScreen.h"

namespace SystemMenu
{

std::unique_ptr<Screen> MakeScreen(ScreenId screen, int argument, MenuServices& services)
{
    switch (screen)
    {
    case ScreenId::HealthAndSafety:
        return std::make_unique<WelcomeScreen>(services);

    case ScreenId::MainFrame:
        return std::make_unique<MainFrameScreen>(services);

    case ScreenId::BottomBar:
        return std::make_unique<BottomBarScreen>(services);

    case ScreenId::BarOverlay:
        return std::make_unique<BarOverlayScreen>(services);

    case ScreenId::ChannelGrid:
        return std::make_unique<ChannelGridScreen>(services);

    case ScreenId::ChannelStartWindow:
        return std::make_unique<ChannelStartWindowScreen>(services);

    case ScreenId::BackToWiiMenu:
        return std::make_unique<BackToWiiMenuScreen>(services);

    case ScreenId::LimitOver:
        return std::make_unique<LimitOverScreen>(services);

    case ScreenId::StartupRouter:
        return std::make_unique<StartupRouterScreen>(services, argument);

    case ScreenId::WiiOptionsBackground:
        return std::make_unique<WiiOptionsBackgroundScreen>(services, argument);

    case ScreenId::WiiOptionsBar:
        return std::make_unique<WiiOptionsBarScreen>(services);

    case ScreenId::WiiOptionsChoices:
        return std::make_unique<WiiOptionsChoicesScreen>(services, argument);

    case ScreenId::WiiSettings:
        return MakeWiiSettingsScreen(services, argument);

    case ScreenId::ChannelDataEditor:
        return std::make_unique<DataEditorScreen>(services, DataKind::Channels);

    case ScreenId::WiiSaveDataEditor:
        return std::make_unique<DataEditorScreen>(services, DataKind::SaveData);

    case ScreenId::SdChannelList:
        return std::make_unique<SdChannelListScreen>(services);

    case ScreenId::SdChannelWindow:
        return std::make_unique<SdChannelWindowScreen>(services);

    case ScreenId::SdCardMenuBar:
        return std::make_unique<SdCardMenuBarScreen>(services);

    default:
        return nullptr;
    }
}

}
