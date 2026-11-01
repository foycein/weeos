#include "SystemMenu/WiiMenu/MainFrameScreen.h"

#include "SystemMenu/Fader.h"
#include "SystemMenu/HomeMenu/HomeButton.h"
#include "SystemMenu/WiiMenu/BottomBarScreen.h"
#include "SystemMenu/MenuFiles.h"
#include "SystemMenu/MessageBoard/MessageBoardData.h"
#include "SystemMenu/ScreenManager.h"
#include "SystemMenu/WiiMenu/ClockFace.h"
#include "SystemMenu/WiiMenu/WiiMenuData.h"

#include <array>

namespace SystemMenu
{

namespace
{

constexpr std::string_view BundleName = "board";
constexpr std::string_view LayoutName = "my_IplTop_c";
constexpr std::string_view AnimationName = "my_IplTop_c";

// The background has three pictures side by side, for yesterday, today and tomorrow. The Wii Menu shows the middle one.
constexpr std::array<std::string_view, 3> DatePanes = {"T_Day_a", "T_Day_b", "T_Day_c"};
constexpr int FirstDayOffset = -1;

// The first frame of the animation of the background shows today in the middle.
constexpr float FrameOfToday = 0;

}

MainFrameScreen::MainFrameScreen(MenuServices& menuServices)
    : Screen(ScreenId::MainFrame)
    , services(menuServices)
{
}

void MainFrameScreen::Load()
{
    archive = services.files.OpenLayoutBundle(BundleName);
}

void MainFrameScreen::Build()
{
    background = std::make_unique<MenuLayout>(services.display, *archive, std::string(LayoutName));
    backgroundAnimation = std::make_unique<FramePlayer>(*background, *archive, AnimationName);
    backgroundAnimation->Show(FrameOfToday);
    WriteTheDates(services.wiiMenu.console.GetLocalTime());
    if (services.messageBoard != nullptr)
        board = std::make_unique<BoardView>(services, *services.messageBoard, *archive, *background, *backgroundAnimation);
}

BoardView* MainFrameScreen::Board()
{
    if (!board || !board->IsShown())
        return nullptr;
    return board.get();
}

void MainFrameScreen::Draw(DrawPass pass)
{
    if (pass != DrawPass::Main || !background)
        return;

    background->Draw();
    if (board)
        board->Draw();
}

bool MainFrameScreen::UpdatesWithChildren() const
{
    return true;
}

bool MainFrameScreen::DrawsWithChildren() const
{
    return true;
}

// The original opens the bar, the grid and the bar overlay in this order, once the channels of the pages around the page
// that shows are loaded, and from then on HOME opens the HOME menu (0x813902F4). WeeOS reads the channels when the grid is built.
void MainFrameScreen::OnStart()
{
    services.homeButton.Start();
    services.screens.OpenChild(ScreenId::BottomBar, ScreenId::MainFrame, 0);
    services.screens.OpenChild(ScreenId::ChannelGrid, ScreenId::MainFrame, 0);
    services.screens.OpenChild(ScreenId::BarOverlay, ScreenId::MainFrame, 0);
}

bool MainFrameScreen::RunMain()
{
    backgroundAnimation->Step();
    if (board && board->IsShown())
    {
        board->Run();
        background->Calculate();
        if (!board->IsShown())
        {
            dayOfTheDates.reset();
            hasShownTheMessageCount = false;
        }
        return false;
    }

    LocalTime today = services.wiiMenu.console.GetLocalTime();
    if (dayOfTheDates != today.day)
        WriteTheDates(today);

    background->Calculate();
    ShowTheMessagesOfToday();
    OpenTheBoardWhenTheGridHasGone();

    if (!hasFadedIn && AreTheChildrenBuilt())
        hasFadedIn = services.fader.StartFadeIn();

    return false;
}

void MainFrameScreen::WriteTheDates(const LocalTime& today)
{
    int dayOffset = FirstDayOffset;
    for (std::string_view datePane : DatePanes)
    {
        LocalTime day = ShiftDays(today, dayOffset);
        background->SetText(datePane, MakeDateText(day, services.messages));
        ++dayOffset;
    }
    dayOfTheDates = today.day;
}

// The envelope counts the messages of today (0x81391904); the bar shows the number while the channels are shown.
void MainFrameScreen::ShowTheMessagesOfToday()
{
    if (services.messageBoard == nullptr || hasShownTheMessageCount)
        return;

    auto* bar = dynamic_cast<BottomBarScreen*>(services.screens.FindScreen(ScreenId::BottomBar));
    if (bar == nullptr)
        return;

    BoardDay today = DayOf(services.wiiMenu.console.GetLocalTime());
    bar->ShowMessageCount(services.messageBoard->store.CountMessagesOfDay(today));
    hasShownTheMessageCount = true;
}

void MainFrameScreen::OpenTheBoardWhenTheGridHasGone()
{
    if (!board || !services.messageBoard->isComingFromTheChannels)
        return;

    bool isGridThere = services.screens.FindScreen(ScreenId::ChannelGrid) != nullptr;
    if (isGridThere || services.screens.IsLoadingScreen())
        return;

    services.messageBoard->isComingFromTheChannels = false;
    board->Open();
}

// The original fades in when the bar overlay, the last of the three, is there.
bool MainFrameScreen::AreTheChildrenBuilt()
{
    return services.screens.FindScreen(ScreenId::BarOverlay) != nullptr;
}

}
