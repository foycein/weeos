#include "SystemMenu/CursorManager.h"

#include <algorithm>
#include <format>

namespace SystemMenu
{

namespace
{

constexpr std::string_view BundleName = "cursor";
constexpr std::string_view ScrollArrowLayoutName = "my_BScroll_a";

// The panes of a hand: one moves it, two turn it (the hand and its shadow).
constexpr std::string_view HandMoverPane = "N_Trans";
constexpr std::string_view HandTurnerPane = "N_Rot";
constexpr std::string_view ShadowTurnerPane = "N_SRot";

// The panes of the scroll arrow: the base, the arrow and the window that is as long as the arrow.
constexpr std::string_view ArrowBasePane = "BArwBase";
constexpr std::string_view ArrowPane = "N_BArw";
constexpr std::string_view ArrowLengthPane = "W_BArw";

constexpr float PointsUpScale = 1;
constexpr float PointsDownScale = -1;

}

CursorManager::CursorManager(Display& menuDisplay, const MenuFiles& files)
    : display(menuDisplay)
    , archive(files.OpenLayoutBundle(BundleName))
{
    for (int playerIndex = 0; playerIndex < PlayerCount; ++playerIndex)
    {
        int playerNumber = playerIndex + 1;
        pointingHands[playerIndex] = std::make_unique<MenuLayout>(display, *archive, std::format("P{}_Def", playerNumber));
        grabbingHands[playerIndex] = std::make_unique<MenuLayout>(display, *archive, std::format("P{}_Cat", playerNumber));
    }
    scrollArrowLayout = std::make_unique<MenuLayout>(display, *archive, std::string(ScrollArrowLayoutName));
}

void CursorManager::SetVisible(bool shouldBeVisible)
{
    isVisible = shouldBeVisible;
}

void CursorManager::SetHandHidden(int playerIndex, bool shouldBeHidden)
{
    hands.at(playerIndex).isHidden = shouldBeHidden;
}

void CursorManager::SetHandGrabbing(int playerIndex, bool shouldGrab)
{
    hands.at(playerIndex).isGrabbing = shouldGrab;
}

void CursorManager::ShowScrollArrow(const ScrollArrow& arrow)
{
    scrollArrow = arrow;
}

void CursorManager::HideScrollArrow()
{
    scrollArrow.reset();
}

void CursorManager::Update(const InputManager& input)
{
    for (int playerIndex = 0; playerIndex < PlayerCount; ++playerIndex)
        UpdateHand(playerIndex, input.Player(playerIndex));

    if (scrollArrow)
        UpdateScrollArrow();
}

void CursorManager::UpdateHand(int playerIndex, const std::optional<PlayerInput>& playerInput)
{
    Hand& hand = hands[playerIndex];
    hand.shownLayout = nullptr;

    if (!playerInput || !playerInput->state.pointerIsValid)
        return;

    MenuLayout& handLayout = hand.isGrabbing ? *grabbingHands[playerIndex] : *pointingHands[playerIndex];
    PlaceHand(handLayout, playerInput->state);
    handLayout.Calculate();
    hand.shownLayout = &handLayout;
}

void CursorManager::PlaceHand(MenuLayout& handLayout, const Input::RemoteState& state)
{
    Formats::Vector2 position = KeepNearPicture(display.PointerToCanvas(state.pointer));
    Formats::Vector2 positionInPane = display.CanvasToRootPaneSpace(position);
    handLayout.PaneNamed(HandMoverPane).translation = {positionInPane.x, positionInPane.y, 0};

    Formats::Vector3 turn = {0, 0, state.rollDegrees};
    handLayout.PaneNamed(HandTurnerPane).rotation = turn;
    handLayout.PaneNamed(ShadowTurnerPane).rotation = turn;
}

Formats::Vector2 CursorManager::KeepNearPicture(Formats::Vector2 position) const
{
    Graphics::Canvas canvas = display.GetCanvas();
    float farthestX = canvas.width / 2 + HandMarginBeyondPicture;
    float farthestY = canvas.height / 2 + HandMarginBeyondPicture;
    return {std::clamp(position.x, -farthestX, farthestX), std::clamp(position.y, -farthestY, farthestY)};
}

void CursorManager::UpdateScrollArrow()
{
    Formats::Vector2 positionInPane = display.CanvasToRootPaneSpace(scrollArrow->position);
    Formats::Vector3 translation = {positionInPane.x, positionInPane.y, 0};

    Formats::Pane& base = scrollArrowLayout->PaneNamed(ArrowBasePane);
    Formats::Pane& arrow = scrollArrowLayout->PaneNamed(ArrowPane);
    base.translation = translation;
    arrow.translation = translation;

    arrow.scale.y = scrollArrow->pointsDown ? PointsDownScale : PointsUpScale;
    arrow.isVisible = scrollArrow->isArrowShown;
    base.isVisible = !scrollArrow->isArrowShown;

    scrollArrowLayout->PaneNamed(ArrowLengthPane).height = std::clamp(scrollArrow->length, MinimumArrowLength, MaximumArrowLength);
    scrollArrowLayout->Calculate();
}

void CursorManager::Draw()
{
    if (!isVisible)
        return;

    // Player 1 is drawn last, so that its hand lies on top of the others.
    for (int playerIndex = PlayerCount - 1; playerIndex >= 0; --playerIndex)
    {
        const Hand& hand = hands[playerIndex];
        if (hand.shownLayout != nullptr && !hand.isHidden)
            hand.shownLayout->Draw();
    }

    if (scrollArrow)
        scrollArrowLayout->Draw();
}

}
