#include "SystemMenu/WiiMenu/BottomBarScreen.h"

#include "SystemMenu/MenuFiles.h"
#include "SystemMenu/ScreenManager.h"
#include "SystemMenu/WiiMenu/LayoutParts.h"

#include "Layout/LiveLayout.h"

#include <algorithm>
#include <array>
#include <string>

namespace SystemMenu
{

namespace
{

constexpr std::string_view BundleName = "cmnBtn";
constexpr std::string_view LayoutName = "my_IplTop_e";
constexpr std::string_view AnimationName = "my_IplTop_e";

// One animation file holds every state of the bar, each in its own frames. The groups below show frame 0, the look of the Wii Menu.
constexpr std::array<std::string_view, 8> GroupsOfTheBar = {
    "G_Bbs", "G_Ch", "G_Set", "G_Cal", "G_Add", "G_AddExit", "G_ArwR_Focus", "G_ArwL_Focus"};
constexpr float FrameOfTheWiiMenu = 0;

// The group that moves the buttons of the bar from one look into another, and the parts of the file for each change (0x8160F7B8).
constexpr std::string_view LookGroup = "G_SeenChange";

struct ChangeFrames
{
    BarChange change;
    float firstFrame;
    float lastFrame;
};

constexpr std::array<ChangeFrames, 13> FramesOfTheChanges = {{
    {BarChange::ToTheBoard, 1000, 1040},
    {BarChange::ToTheChannels, 6000, 6040},
    {BarChange::CalendarOpens, 2000, 2050},
    {BarChange::CalendarClosesWithBack, 3000, 3050},
    {BarChange::CalendarClosesWithADay, 3500, 3550},
    {BarChange::CreateMessageOpens, 4000, 4026},
    {BarChange::LetterOpens, 3100, 3113},
    {BarChange::BackToTheBoard, 3426, 3439},
    {BarChange::PlayLogShows, 3113, 3126},
    {BarChange::LetterShowsWithoutButton, 3600, 3613},
    {BarChange::MemoWriterShows, 3313, 3326},
    {BarChange::MemoWriterCloses, 3413, 3426},
    {BarChange::LetterShowsWithButton, 3640, 3653},
}};

// A pressed button of the bar flashes (0x8139CB58 with 0x1B, 0x1C and 0x1D).
constexpr std::string_view BackFlashGroup = "G_CalExit";
constexpr std::string_view TrashFlashGroup = "G_Dust";
constexpr std::string_view RightFlashGroup = "G_Cmn_R";
constexpr float FirstFrameOfAButtonFlash = 3000;
constexpr float LastFrameOfAButtonFlash = 3020;
constexpr float FirstFrameOfTheTrashFlash = 2800;
constexpr float LastFrameOfTheTrashFlash = 2820;

// The number of the messages of today on the envelope blinks in frames 1 to 400 of its group (0x8139D004).
constexpr std::string_view MessageSignalGroup = "G_BbsSignal";
constexpr std::string_view MessageCountText = "T_BbsMark1";
constexpr float FirstFrameOfTheSignal = 1;
constexpr float LastFrameOfTheSignal = 400;
constexpr int LargestMessageCount = 99;

constexpr std::string_view BackText = "T_CalExit";
constexpr std::string_view RightText = "T_CalAdd_R";
constexpr std::string_view TrashText = "T_Dust";

// The arrows bob sideways in frames 10000 to 10055 of the file, again and again.
constexpr std::string_view ArrowLoopGroup = "G_ArwRoop";
constexpr float FirstFrameOfTheArrowLoop = 10000;
constexpr float LastFrameOfTheArrowLoop = 10055;

// An arrow without a page behind it slides out of the picture in frames 10100 to 10110 and comes back in frames 10150 to 10160.
constexpr std::string_view PreviousPageArrowGroup = "G_ArwL_End";
constexpr std::string_view NextPageArrowGroup = "G_ArwR_End";
constexpr float FirstFrameOfSlidingOut = 10100;
constexpr float LastFrameOfSlidingOut = 10110;
constexpr float FirstFrameOfSlidingIn = 10150;
constexpr float LastFrameOfSlidingIn = 10160;

// The bar never draws its arrows: they would lie under the channel grid. The bar overlay draws them over it.
constexpr std::string_view PreviousPageArrow = "ArwL";
constexpr std::string_view NextPageArrow = "ArwR";

struct ButtonPane
{
    std::string_view paneName;
    BarButton button;
};

constexpr std::array<ButtonPane, 10> ButtonPanes = {{
    {"B_Set", BarButton::WiiButton},
    {"B_Bbs", BarButton::MessageBoardButton},
    {"B_ArwL", BarButton::PreviousPage},
    {"B_ArwR", BarButton::NextPage},
    {"B_Cal", BarButton::CalendarButton},
    {"B_Add", BarButton::CreateMessageButton},
    {"B_Ch", BarButton::ChannelsButton},
    {"B_CalExit", BarButton::BackButton},
    {"B_Dust", BarButton::TrashButton},
    {"B_Add_R", BarButton::RightButton},
}};

std::u16string NumberText(int number)
{
    std::string digits = std::to_string(number);
    return std::u16string(digits.begin(), digits.end());
}

}

BottomBarScreen::BottomBarScreen(MenuServices& menuServices)
    : Screen(ScreenId::BottomBar)
    , services(menuServices)
{
}

void BottomBarScreen::Load()
{
    archive = services.files.OpenLayoutBundle(BundleName);
}

void BottomBarScreen::Build()
{
    layout = std::make_unique<MenuLayout>(services.display, *archive, std::string(LayoutName));

    for (std::string_view group : GroupsOfTheBar)
    {
        stillGroups.push_back(MakeGroupPlayer(group));
        stillGroups.back()->Show(FrameOfTheWiiMenu);
    }

    seenChange = MakeGroupPlayer(LookGroup);
    backFlash = MakeGroupPlayer(BackFlashGroup);
    trashFlash = MakeGroupPlayer(TrashFlashGroup);
    rightFlash = MakeGroupPlayer(RightFlashGroup);
    messageSignal = MakeGroupPlayer(MessageSignalGroup);
    for (FramePlayer* player : {seenChange.get(), backFlash.get(), trashFlash.get(), rightFlash.get(), messageSignal.get()})
        player->Show(FrameOfTheWiiMenu);

    arrowLoop = MakeGroupPlayer(ArrowLoopGroup);
    arrowLoop->Loop(FirstFrameOfTheArrowLoop, LastFrameOfTheArrowLoop);

    previousPageArrow = MakeGroupPlayer(PreviousPageArrowGroup);
    nextPageArrow = MakeGroupPlayer(NextPageArrowGroup);
    previousPageArrow->Show(LastFrameOfSlidingOut);
    nextPageArrow->Show(LastFrameOfSlidingOut);

    layout->PaneNamed(PreviousPageArrow).isVisible = false;
    layout->PaneNamed(NextPageArrow).isVisible = false;
    layout->Calculate();
}

std::unique_ptr<FramePlayer> BottomBarScreen::MakeGroupPlayer(std::string_view groupName)
{
    return std::make_unique<FramePlayer>(*layout, *archive, AnimationName, layout->Live().PanesOfGroup(groupName));
}

void BottomBarScreen::Draw(DrawPass pass)
{
    if (pass != DrawPass::Main || !layout)
        return;

    layout->Draw();
}

void BottomBarScreen::DrawPageArrows()
{
    DrawArrow(PreviousPageArrow);
    DrawArrow(NextPageArrow);
}

void BottomBarScreen::DrawArrow(std::string_view arrowName)
{
    Formats::Pane& arrow = layout->PaneNamed(arrowName);
    std::vector<Formats::Pane*> panesOfTheArrow = Layout::PaneWithDescendants(arrow);
    auto isPartOfTheArrow = [&](const Formats::Pane& pane) {
        return std::find(panesOfTheArrow.begin(), panesOfTheArrow.end(), &pane) != panesOfTheArrow.end();
    };

    arrow.isVisible = true;
    DrawPanes(*layout, services.display, PlacedPanes(*layout), isPartOfTheArrow);
    arrow.isVisible = false;
}

void BottomBarScreen::ShowPageArrows(bool hasPreviousPage, bool hasNextPage)
{
    SlideArrow(*previousPageArrow, isPreviousPageArrowShown, hasPreviousPage);
    SlideArrow(*nextPageArrow, isNextPageArrowShown, hasNextPage);
}

void BottomBarScreen::SlideArrow(FramePlayer& arrow, bool& isShown, bool shouldBeShown)
{
    if (isShown == shouldBeShown)
        return;

    isShown = shouldBeShown;
    if (isShown)
        arrow.PlayOnce(FirstFrameOfSlidingIn, LastFrameOfSlidingIn);
    else
        arrow.PlayOnce(FirstFrameOfSlidingOut, LastFrameOfSlidingOut);
}

bool BottomBarScreen::IsPreviousPageArrowShown() const
{
    return isPreviousPageArrowShown;
}

bool BottomBarScreen::IsNextPageArrowShown() const
{
    return isNextPageArrowShown;
}

// A bar that is changing its look has no buttons.
std::optional<BarButton> BottomBarScreen::ButtonAt(Formats::Vector2 place)
{
    if (IsChanging())
        return std::nullopt;

    for (const Formats::Pane* pane : layout->BoundingPanesAt(place))
    {
        for (const ButtonPane& buttonPane : ButtonPanes)
        {
            if (pane->name != buttonPane.paneName || !HasButtonInThisMode(buttonPane.button))
                continue;

            if (buttonPane.button == BarButton::PreviousPage && !isPreviousPageArrowShown)
                continue;

            if (buttonPane.button == BarButton::NextPage && !isNextPageArrowShown)
                continue;

            return buttonPane.button;
        }
    }
    return std::nullopt;
}

Formats::Vector2 BottomBarScreen::PlaceOfButton(BarButton button)
{
    PlacedPanes placedPanes(*layout);
    for (const ButtonPane& buttonPane : ButtonPanes)
    {
        if (buttonPane.button == button)
            return placedPanes.PlaceOf(buttonPane.paneName).value_or(Formats::Vector2{0, 0});
    }
    return {0, 0};
}

bool BottomBarScreen::HasButtonInThisMode(BarButton button) const
{
    bool isPageArrow = button == BarButton::PreviousPage || button == BarButton::NextPage;
    switch (mode)
    {
    case BarMode::WiiMenu:
        return isPageArrow || button == BarButton::WiiButton || button == BarButton::MessageBoardButton;

    case BarMode::MessageBoard:
        return isPageArrow || button == BarButton::CalendarButton || button == BarButton::CreateMessageButton ||
            button == BarButton::ChannelsButton;

    case BarMode::Letter:
        return button == BarButton::BackButton || button == BarButton::TrashButton;

    case BarMode::LetterWithRightButton:
        return button == BarButton::BackButton || button == BarButton::TrashButton || button == BarButton::RightButton;

    case BarMode::Calendar:
        return isPageArrow || button == BarButton::BackButton;

    case BarMode::MemoWriter:
        return button == BarButton::BackButton || button == BarButton::RightButton;
    }
    return false;
}

void BottomBarScreen::SetMode(BarMode newMode)
{
    mode = newMode;
}

BarMode BottomBarScreen::Mode() const
{
    return mode;
}

void BottomBarScreen::Change(BarChange change)
{
    waitingChanges.push_back(change);
}

bool BottomBarScreen::IsChanging() const
{
    return seenChange->IsPlaying() || !waitingChanges.empty();
}

void BottomBarScreen::SetBackText(std::u16string_view text)
{
    layout->SetText(BackText, text);
}

void BottomBarScreen::SetRightText(std::u16string_view text)
{
    layout->SetText(RightText, text);
}

void BottomBarScreen::SetTrashText(std::u16string_view text)
{
    layout->SetText(TrashText, text);
}

void BottomBarScreen::FlashButton(BarButton button)
{
    if (button == BarButton::BackButton)
        backFlash->PlayOnce(FirstFrameOfAButtonFlash, LastFrameOfAButtonFlash);
    else if (button == BarButton::RightButton)
        rightFlash->PlayOnce(FirstFrameOfAButtonFlash, LastFrameOfAButtonFlash);
    else if (button == BarButton::TrashButton)
        trashFlash->PlayOnce(FirstFrameOfTheTrashFlash, LastFrameOfTheTrashFlash);
}

void BottomBarScreen::ShowMessageCount(int count)
{
    int shownCount = std::min(count, LargestMessageCount);
    layout->SetText(MessageCountText, shownCount > 0 ? NumberText(shownCount) : std::u16string());
    if (shownCount == 0)
        messageSignal->Show(FrameOfTheWiiMenu);
    else if (!messageSignal->IsPlaying())
        messageSignal->Loop(FirstFrameOfTheSignal, LastFrameOfTheSignal);
}

void BottomBarScreen::StartTheNextChange()
{
    if (seenChange->IsPlaying() || waitingChanges.empty())
        return;

    BarChange next = waitingChanges.front();
    waitingChanges.pop_front();
    for (const ChangeFrames& frames : FramesOfTheChanges)
    {
        if (frames.change == next)
            seenChange->PlayOnce(frames.firstFrame, frames.lastFrame);
    }
}

bool BottomBarScreen::RunMain()
{
    StartTheNextChange();

    // A flash that has ended leaves its button as the Wii Menu shows it.
    for (FramePlayer* flash : {backFlash.get(), trashFlash.get(), rightFlash.get()})
    {
        if (!flash->IsPlaying() && flash->Frame() != FrameOfTheWiiMenu)
            flash->Show(FrameOfTheWiiMenu);
        flash->Step();
    }

    seenChange->Step();
    messageSignal->Step();
    arrowLoop->Step();
    previousPageArrow->Step();
    nextPageArrow->Step();
    layout->Calculate();
    return false;
}

BarOverlayScreen::BarOverlayScreen(MenuServices& menuServices)
    : Screen(ScreenId::BarOverlay)
    , services(menuServices)
{
}

void BarOverlayScreen::Draw(DrawPass pass)
{
    if (pass != DrawPass::Main)
        return;

    auto* bar = dynamic_cast<BottomBarScreen*>(services.screens.FindScreen(ScreenId::BottomBar));
    if (bar != nullptr)
        bar->DrawPageArrows();
}

}
