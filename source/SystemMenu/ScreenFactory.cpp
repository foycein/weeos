#include "SystemMenu/ScreenFactory.h"

#include "SystemMenu/BackToWiiMenuScreen.h"
#include "SystemMenu/DataManagement/DataEditorScreen.h"
#include "SystemMenu/HealthAndSafetyScreen.h"
#include "SystemMenu/LimitOverScreen.h"
#include "SystemMenu/ParentalDialogScreen.h"
#include "SystemMenu/StartupRouterScreen.h"
#include "SystemMenu/WiiOptionsBackgroundScreen.h"
#include "SystemMenu/WiiOptionsBarScreen.h"
#include "SystemMenu/WiiOptionsChoicesScreen.h"
#include "SystemMenu/WiiSettings/SettingsScreen.h"

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

    default:
        return nullptr;
    }
}

}
