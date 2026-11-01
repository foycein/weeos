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
constexpr std::array<std::string_view, 8> StillGroupsOfTheBar = {
    "G_SeenChange", "G_Ch", "G_Cal", "G_Add", "G_CalExit", "G_AddExit", "G_Cmn_R", "G_Dust"};
constexpr float FrameOfTheWiiMenu = 0;

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

constexpr std::array<FocusButton, 4> FocusButtons = {{
    {BarButton::MessageBoardButton, "B_Bbs", "G_Bbs", 900, 906, 930, 938, 15},
    {BarButton::WiiButton, "B_Set", "G_Set", 6900, 6906, 6930, 6938, 14},
    {BarButton::NextPage, "B_ArwR", "G_ArwR_Focus", 10600, 10615, 10800, 10815, std::nullopt},
    {BarButton::PreviousPage, "B_ArwL", "G_ArwL_Focus", 10600, 10615, 10800, 10815, std::nullopt},
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
// 120 units from the edges of the 16:9 picture (0x813D1AB8).
constexpr int StepsBeforeABalloonAppears = 17;
constexpr float BalloonHeightAboveTheButton = 50;
constexpr float BalloonMarginFromTheEdges = 120;

constexpr std::string_view TargetedSound = "WIPL_SE_BT_TARGETTING";
constexpr std::string_view BalloonSound = "WIPL_SE_BALLOON";
constexpr std::string_view CardInSound = "WIPL_SE_SDCARD_IN";
constexpr std::string_view CardOutSound = "WIPL_SE_SDCARD_OUT";

// The message "SD Card Menu" (0x81694BA0).
constexpr std::uint16_t SdCardMenuMessage = 161;

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

std::optional<BarButton> BottomBarScreen::ButtonAt(Formats::Vector2 place)
{
    if (sdCardButton->IsAt(place))
        return BarButton::SdCardButton;

    for (const Formats::Pane* pane : layout->BoundingPanesAt(place))
    {
        for (const FocusButton& focusButton : FocusButtons)
        {
            if (pane->name == focusButton.paneName && IsShown(focusButton.button))
                return focusButton.button;
        }
    }
    return std::nullopt;
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
    for (std::size_t number = 0; number < FocusButtons.size(); ++number)
    {
        FocusState& state = focusStates[number];
        if (!state.balloon)
            continue;

        Formats::Vector2 button = PlaceOfButton(FocusButtons[number].button);
        state.balloon->PlaceAt({button.x, button.y + BalloonHeightAboveTheButton}, BalloonMarginFromTheEdges);
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
