#include "SystemMenu/WiiMenu/BottomBarScreen.h"

#include "SystemMenu/Fader.h"
#include "SystemMenu/InputManager.h"
#include "SystemMenu/MenuFiles.h"
#include "SystemMenu/ScreenManager.h"
#include "SystemMenu/SoundRequests.h"
#include "SystemMenu/WiiMenu/LayoutParts.h"

#include "Ios/SdCard/SdCardService.h"
#include "Layout/LiveLayout.h"
#include "SystemMenu/DataManagement/DataManagementServices.h"
#include "Text/MessageCatalog.h"

#include <algorithm>
#include <string>

namespace SystemMenu
{

namespace
{

constexpr std::string_view BundleName = "cmnBtn";
constexpr std::string_view BalloonBundleName = "balloon";
constexpr std::string_view LayoutName = "my_IplTop_e";
constexpr std::string_view AnimationName = "my_IplTop_e";

// One animation file holds every state of the bar, each in its own frames. The groups below show frame 0, the look of the Wii Menu.
// The groups of the buttons that answer to the hands are in FocusButtons.
constexpr std::array<std::string_view, 1> StillGroupsOfTheBar = {"G_AddExit"};
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

// The number of the messages of today on the envelope blinks in frames 1 to 400 of its group (0x8139D004).
constexpr std::string_view MessageSignalGroup = "G_BbsSignal";
constexpr std::string_view MessageCountText = "T_BbsMark1";
constexpr float FirstFrameOfTheSignal = 1;
constexpr float LastFrameOfTheSignal = 400;
constexpr int LargestMessageCount = 99;

constexpr std::string_view BackText = "T_CalExit";
constexpr std::string_view RightText = "T_CalAdd_R";
constexpr std::string_view TrashText = "T_Dust";

// A button that a hand comes onto grows to 110 percent in the first frames of its part of the file and shrinks back in the second
// (the table 0x8160F918 of the original), on the panes of its group.
struct FocusButton
{
    BarButton button;
    std::string_view paneName;
    std::string_view group;
    float firstFrameOfFocusIn;
    float lastFrameOfFocusIn;
    float firstFrameOfFocusOut;
    float lastFrameOfFocusOut;

    // The message of the balloon: 15 "Wii Message Board", 14 "Wii Options" (the table 0x8160F9C8). The arrows have none.
    std::optional<std::uint16_t> balloonMessage;
};

constexpr std::array<FocusButton, 10> FocusButtons = {{
    {BarButton::MessageBoardButton, "B_Bbs", "G_Bbs", 900, 906, 930, 938, 15},
    {BarButton::WiiButton, "B_Set", "G_Set", 6900, 6906, 6930, 6938, 14},
    {BarButton::NextPage, "B_ArwR", "G_ArwR_Focus", 10600, 10615, 10800, 10815, std::nullopt},
    {BarButton::PreviousPage, "B_ArwL", "G_ArwL_Focus", 10600, 10615, 10800, 10815, std::nullopt},
    {BarButton::ChannelsButton, "B_Ch", "G_Ch", 5900, 5906, 5930, 5938, std::nullopt},
    {BarButton::CalendarButton, "B_Cal", "G_Cal", 1900, 1906, 1930, 1938, std::nullopt},
    {BarButton::CreateMessageButton, "B_Add", "G_Add", 3900, 3906, 3930, 3938, std::nullopt},
    {BarButton::BackButton, "B_CalExit", "G_CalExit", 2900, 2906, 2930, 2938, std::nullopt},
    {BarButton::RightButton, "B_Add_R", "G_Cmn_R", 2900, 2906, 2930, 2938, std::nullopt},
    {BarButton::TrashButton, "B_Dust", "G_Dust", 2900, 2909, 2930, 2938, std::nullopt},
}};

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

// A pressed arrow flashes in frames 10700 to 10730 (the bar events 0x13 and 0x14).
constexpr std::string_view PreviousPageFlashGroup = "G_ArwL_Ac";
constexpr std::string_view NextPageFlashGroup = "G_ArwR_Ac";
constexpr float FirstFrameOfTheFlash = 10700;
constexpr float LastFrameOfTheFlash = 10730;

// The bar never draws its arrows: they would lie under the channel grid. The bar overlay draws them over it.
constexpr std::string_view PreviousPageArrow = "ArwL";
constexpr std::string_view NextPageArrow = "ArwR";

// A balloon of the bar appears 17 steps after the hand came (0x813D18F8), 50 units above its button (0x8139CF4C), and keeps
// 120 units from the edges of a 16:9 picture or 30 units from those of a 4:3 picture (0x813D1AB8).
constexpr int StepsBeforeABalloonAppears = 17;
constexpr float BalloonHeightAboveTheButton = 50;
constexpr float BalloonMarginOnAWideScreen = 120;
constexpr float BalloonMarginOnANarrowScreen = 30;

constexpr std::string_view TargetedSound = "WIPL_SE_BT_TARGETTING";
constexpr std::string_view BalloonSound = "WIPL_SE_BALLOON";
constexpr std::string_view CardInSound = "WIPL_SE_SDCARD_IN";
constexpr std::string_view CardOutSound = "WIPL_SE_SDCARD_OUT";

// The message "SD Card Menu" (0x81694BA0).
constexpr std::uint16_t SdCardMenuMessage = 161;

std::u16string NumberText(int number)
{
    std::string digits = std::to_string(number);
    return std::u16string(digits.begin(), digits.end());
}

std::size_t NumberOf(BarButton button)
{
    for (std::size_t number = 0; number < FocusButtons.size(); ++number)
    {
        if (FocusButtons[number].button == button)
            return number;
    }
    return 0;
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
    balloonArchive = services.files.OpenLayoutBundle(BalloonBundleName);
}

void BottomBarScreen::Build()
{
    layout = std::make_unique<MenuLayout>(services.display, *archive, std::string(LayoutName));

    for (std::string_view group : StillGroupsOfTheBar)
    {
        stillGroups.push_back(std::make_unique<FramePlayer>(*layout, *archive, AnimationName, layout->Live().PanesOfGroup(group)));
        stillGroups.back()->Show(FrameOfTheWiiMenu);
    }
    MakeTheFocusStates();

    seenChange = std::make_unique<FramePlayer>(*layout, *archive, AnimationName, layout->Live().PanesOfGroup(LookGroup));
    seenChange->Show(FrameOfTheWiiMenu);
    messageSignal = std::make_unique<FramePlayer>(*layout, *archive, AnimationName, layout->Live().PanesOfGroup(MessageSignalGroup));
    messageSignal->Show(FrameOfTheWiiMenu);

    // The layout holds "99" under the envelope until the main frame counts the messages of today; a menu without a board shows none.
    ShowMessageCount(0);

    arrowLoop = std::make_unique<FramePlayer>(*layout, *archive, AnimationName, layout->Live().PanesOfGroup(ArrowLoopGroup));
    arrowLoop->Loop(FirstFrameOfTheArrowLoop, LastFrameOfTheArrowLoop);

    previousPageArrow = std::make_unique<FramePlayer>(*layout, *archive, AnimationName, layout->Live().PanesOfGroup(PreviousPageArrowGroup));
    nextPageArrow = std::make_unique<FramePlayer>(*layout, *archive, AnimationName, layout->Live().PanesOfGroup(NextPageArrowGroup));
    previousPageArrow->Show(LastFrameOfSlidingOut);
    nextPageArrow->Show(LastFrameOfSlidingOut);

    previousPageFlash = std::make_unique<FramePlayer>(*layout, *archive, AnimationName, layout->Live().PanesOfGroup(PreviousPageFlashGroup));
    nextPageFlash = std::make_unique<FramePlayer>(*layout, *archive, AnimationName, layout->Live().PanesOfGroup(NextPageFlashGroup));
    previousPageFlash->Show(FrameOfTheWiiMenu);
    nextPageFlash->Show(FrameOfTheWiiMenu);

    layout->PaneNamed(PreviousPageArrow).isVisible = false;
    layout->PaneNamed(NextPageArrow).isVisible = false;
    layout->Calculate();

    sdCardButton = std::make_unique<SdCardButton>(services.display, *archive, *balloonArchive, services.messages.Find(SdCardMenuMessage));
    wasCardInTheSlot = IsCardInTheSlot();
    sdCardButton->ShowCardInserted(wasCardInTheSlot);
    sdCardButton->Appear();
}

void BottomBarScreen::MakeTheFocusStates()
{
    for (const FocusButton& focusButton : FocusButtons)
    {
        FocusState state;
        state.focus = std::make_unique<FramePlayer>(*layout, *archive, AnimationName, layout->Live().PanesOfGroup(focusButton.group));
        state.focus->Show(FrameOfTheWiiMenu);
        if (focusButton.balloonMessage)
        {
            state.balloon = std::make_unique<Balloon>(services.display, *balloonArchive, StepsBeforeABalloonAppears);
            state.balloon->SetText(services.messages.Find(*focusButton.balloonMessage));
        }
        focusStates.push_back(std::move(state));
    }
}

// The bar zooms with the grid: it is seen through the camera of the display (0x8139C49C). Its arrows are not (0x8139DA5C).
void BottomBarScreen::Draw(DrawPass pass)
{
    if (pass != DrawPass::Main || !layout)
        return;

    services.display.LookThroughCamera();
    layout->Draw();
    if (mode == BarMode::WiiMenu)
        sdCardButton->Draw();
    services.display.StopMovingAreas();
}

void BottomBarScreen::DrawPageArrows()
{
    DrawArrow(PreviousPageArrow);
    DrawArrow(NextPageArrow);
}

void BottomBarScreen::DrawBalloons()
{
    for (FocusState& state : focusStates)
    {
        if (state.balloon)
            state.balloon->Draw();
    }
    sdCardButton->DrawBalloon();
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

// An arrow that slides out loses its focus first (the bar events 0x19 and 0x1A).
void BottomBarScreen::SlideArrow(FramePlayer& arrow, bool& isShown, bool shouldBeShown)
{
    if (isShown == shouldBeShown)
        return;

    isShown = shouldBeShown;
    if (isShown)
    {
        arrow.PlayOnce(FirstFrameOfSlidingIn, LastFrameOfSlidingIn);
        return;
    }

    BarButton button = &arrow == previousPageArrow.get() ? BarButton::PreviousPage : BarButton::NextPage;
    SetFocused(NumberOf(button), false);
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

void BottomBarScreen::FlashPageArrow(BarButton arrow)
{
    FramePlayer& flash = arrow == BarButton::PreviousPage ? *previousPageFlash : *nextPageFlash;
    flash.PlayOnce(FirstFrameOfTheFlash, LastFrameOfTheFlash);
}

void BottomBarScreen::PressSdCardButton()
{
    sdCardButton->PlayPress();
}

// The original takes the focus away from every button when it disables them (0x8139C5C0).
void BottomBarScreen::SetButtonsEnabled(bool shouldBeEnabled)
{
    areButtonsEnabled = shouldBeEnabled;
    if (areButtonsEnabled)
        return;

    for (std::size_t number = 0; number < FocusButtons.size(); ++number)
        SetFocused(number, false);

    sdCardButton->SetFocused(false);
}

bool BottomBarScreen::IsFocused(BarButton button) const
{
    if (button == BarButton::SdCardButton)
        return sdCardButton->IsFocused();

    return focusStates.at(NumberOf(button)).isFocused;
}

const Balloon* BottomBarScreen::BalloonOf(BarButton button) const
{
    if (button == BarButton::SdCardButton)
        return &sdCardButton->NameBalloon();

    return focusStates.at(NumberOf(button)).balloon.get();
}

bool BottomBarScreen::IsShown(BarButton button) const
{
    if (button == BarButton::PreviousPage)
        return isPreviousPageArrowShown;

    if (button == BarButton::NextPage)
        return isNextPageArrowShown;

    return true;
}

bool BottomBarScreen::HasButtonInThisMode(BarButton button) const
{
    bool isPageArrow = button == BarButton::PreviousPage || button == BarButton::NextPage;
    switch (mode)
    {
    case BarMode::WiiMenu:
        return isPageArrow || button == BarButton::WiiButton || button == BarButton::MessageBoardButton || button == BarButton::SdCardButton;

    case BarMode::MessageBoard:
        return isPageArrow || button == BarButton::CalendarButton || button == BarButton::CreateMessageButton ||
            button == BarButton::ChannelsButton;

    case BarMode::Letter:
        return button == BarButton::BackButton || button == BarButton::TrashButton;

    case BarMode::LetterWithRightButton:
        return button == BarButton::BackButton || button == BarButton::TrashButton || button == BarButton::RightButton;

    case BarMode::PlayLog:
        return button == BarButton::BackButton;

    case BarMode::Calendar:
        return isPageArrow || button == BarButton::BackButton;

    case BarMode::MemoWriter:
        return button == BarButton::BackButton || button == BarButton::RightButton;
    }
    return false;
}

// A bar that is changing its look has no buttons.
std::optional<BarButton> BottomBarScreen::ButtonAt(Formats::Vector2 place)
{
    if (IsChanging())
        return std::nullopt;

    if (HasButtonInThisMode(BarButton::SdCardButton) && sdCardButton->IsAt(place))
        return BarButton::SdCardButton;

    for (const Formats::Pane* pane : layout->BoundingPanesAt(place))
    {
        for (const FocusButton& focusButton : FocusButtons)
        {
            bool isThisButton = pane->name == focusButton.paneName && HasButtonInThisMode(focusButton.button);
            if (isThisButton && IsShown(focusButton.button))
                return focusButton.button;
        }
    }
    return std::nullopt;
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

Formats::Vector2 BottomBarScreen::PlaceOfButton(BarButton button)
{
    if (button == BarButton::SdCardButton)
        return sdCardButton->Place();

    PlacedPanes placedPanes(*layout);
    return placedPanes.PlaceOf(FocusButtons[NumberOf(button)].paneName).value_or(Formats::Vector2{0, 0});
}

bool BottomBarScreen::RunMain()
{
    StartTheNextChange();
    seenChange->Step();
    messageSignal->Step();
    FocusTheButtonsUnderThePointers();
    PlaceTheBalloons();
    FollowTheSdCard();

    arrowLoop->Step();
    previousPageArrow->Step();
    nextPageArrow->Step();
    previousPageFlash->Step();
    nextPageFlash->Step();
    for (FocusState& state : focusStates)
    {
        state.focus->Step();
        if (!state.balloon)
            continue;

        bool wasBalloonShown = state.balloon->IsShown();
        state.balloon->Calculate();
        if (!wasBalloonShown && state.balloon->IsShown())
            services.sounds.Request(BalloonSound);
    }

    layout->Calculate();
    bool hasSdBalloonAppeared = sdCardButton->Calculate();
    if (hasSdBalloonAppeared)
        services.sounds.Request(BalloonSound);

    return false;
}

// The button turns white when a card comes, rings once and says so; it turns grey when the card goes (0x813AAA38, 0x813EB330).
void BottomBarScreen::FollowTheSdCard()
{
    bool isCardInTheSlot = IsCardInTheSlot();
    if (isCardInTheSlot == wasCardInTheSlot)
        return;

    wasCardInTheSlot = isCardInTheSlot;
    sdCardButton->ShowCardInserted(isCardInTheSlot);
    if (isCardInTheSlot)
    {
        sdCardButton->PlayInsert();
        services.sounds.Request(CardInSound);
        return;
    }

    services.sounds.Request(CardOutSound);
}

bool BottomBarScreen::IsCardInTheSlot() const
{
    return services.dataManagement != nullptr && services.dataManagement->sdCard.GetStatus().isInserted;
}

// The buttons answer only while the picture is fully in view and no channel is starting.
void BottomBarScreen::FocusTheButtonsUnderThePointers()
{
    std::array<bool, FocusButtons.size()> isPointedAt = {};
    bool isSdCardButtonPointedAt = false;
    bool canBeFocused = areButtonsEnabled && services.fader.GetStatus() == FaderStatus::Clear;
    for (int playerIndex = 0; playerIndex < PlayerCount && canBeFocused; ++playerIndex)
    {
        const std::optional<PlayerInput>& player = services.input.Player(playerIndex);
        if (!player || !player->state.pointerIsValid)
            continue;

        std::optional<BarButton> button = ButtonAt(services.display.PointerToCanvas(player->state.pointer));
        if (button == BarButton::SdCardButton)
            isSdCardButtonPointedAt = true;
        else if (button)
            isPointedAt[NumberOf(*button)] = true;
    }

    for (std::size_t number = 0; number < FocusButtons.size(); ++number)
        SetFocused(number, isPointedAt[number]);

    if (isSdCardButtonPointedAt && !sdCardButton->IsFocused())
        services.sounds.Request(TargetedSound);

    sdCardButton->SetFocused(isSdCardButtonPointedAt);
}

// The first hand that comes plays the growing part and WIPL_SE_BT_TARGETTING; when the last hand goes the button shrinks (0x8139C690, 0x8139C7EC).
void BottomBarScreen::SetFocused(std::size_t buttonNumber, bool shouldBeFocused)
{
    FocusState& state = focusStates.at(buttonNumber);
    if (state.isFocused == shouldBeFocused)
        return;

    state.isFocused = shouldBeFocused;
    const FocusButton& focusButton = FocusButtons[buttonNumber];
    if (state.isFocused)
    {
        state.focus->PlayOnce(focusButton.firstFrameOfFocusIn, focusButton.lastFrameOfFocusIn);
        services.sounds.Request(TargetedSound);
    }
    else
    {
        state.focus->PlayOnce(focusButton.firstFrameOfFocusOut, focusButton.lastFrameOfFocusOut);
    }

    if (state.balloon && state.isFocused)
        state.balloon->PointerArrived();
    else if (state.balloon)
        state.balloon->PointerLeft();
}

void BottomBarScreen::PlaceTheBalloons()
{
    bool isWide = services.display.GetScreenShape() == Platform::ScreenShape::SixteenByNine;
    float margin = isWide ? BalloonMarginOnAWideScreen : BalloonMarginOnANarrowScreen;
    for (std::size_t number = 0; number < FocusButtons.size(); ++number)
    {
        FocusState& state = focusStates[number];
        if (!state.balloon)
            continue;

        Formats::Vector2 button = PlaceOfButton(FocusButtons[number].button);
        state.balloon->PlaceAt({button.x, button.y + BalloonHeightAboveTheButton}, margin);
    }
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
    if (bar == nullptr)
        return;

    bar->DrawPageArrows();
    bar->DrawBalloons();
}

}
