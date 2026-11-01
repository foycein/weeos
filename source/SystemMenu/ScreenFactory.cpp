#include "SystemMenu/ScreenFactory.h"

#include "SystemMenu/BackToWiiMenuScreen.h"
#include "SystemMenu/DataManagement/DataEditorScreen.h"
#include "SystemMenu/SdCardMenu/SdCardMenuScreens.h"
#include "SystemMenu/HealthAndSafetyScreen.h"
#include "SystemMenu/LimitOverScreen.h"
#include "SystemMenu/MessageBoard/CalendarScreen.h"
#include "SystemMenu/MessageBoard/MemoWriterScreen.h"
#include "SystemMenu/MessageBoard/MiiChooserScreen.h"
#include "SystemMenu/ParentalDialogScreen.h"
#include "SystemMenu/StartupRouterScreen.h"
#include "SystemMenu/WiiOptionsBackgroundScreen.h"
#include "SystemMenu/WiiOptionsBarScreen.h"
#include "SystemMenu/WiiOptionsChoicesScreen.h"
#include "SystemMenu/WiiMenu/BottomBarScreen.h"
#include "SystemMenu/WiiMenu/ChannelGridScreen.h"
#include "SystemMenu/WiiMenu/MainFrameScreen.h"
#include "SystemMenu/WiiSettings/SettingsScreen.h"

namespace SystemMenu
{

namespace
{

// The screens of the Message Board exist only when the menu has the board.
template <typename BoardScreen>
std::unique_ptr<Screen> MakeMessageBoardScreen(MenuServices& services)
{
    if (services.messageBoard == nullptr)
        return nullptr;
    return std::make_unique<BoardScreen>(services);
}

}

std::unique_ptr<Screen> MakeScreen(ScreenId screen, int argument, MenuServices& services)
{
    switch (screen)
    {
    case ScreenId::HealthAndSafety:
        return std::make_unique<HealthAndSafetyScreen>(services);

    case ScreenId::MainFrame:
        return std::make_unique<MainFrameScreen>(services);

    case ScreenId::BottomBar:
        return std::make_unique<BottomBarScreen>(services);

    case ScreenId::BarOverlay:
        return std::make_unique<BarOverlayScreen>(services);

    case ScreenId::ChannelGrid:
        return std::make_unique<ChannelGridScreen>(services, argument);

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

    case ScreenId::Calendar:
        return MakeMessageBoardScreen<CalendarScreen>(services);

    case ScreenId::MemoWriter:
        return MakeMessageBoardScreen<MemoWriterScreen>(services);

    case ScreenId::MiiChooser:
        return MakeMessageBoardScreen<MiiChooserScreen>(services);

    case ScreenId::SdChannelList:
        return std::make_unique<SdChannelListScreen>(services);

    case ScreenId::SdCardMenuBar:
        return std::make_unique<SdCardMenuBarScreen>(services);

    default:
        return nullptr;
    }
}

}
