#pragma once

#include "SystemMenu/Display.h"
#include "SystemMenu/InputManager.h"
#include "SystemMenu/MenuFiles.h"
#include "SystemMenu/MenuLayout.h"

#include "Formats/Vectors.h"
#include "Layout/LayoutArchive.h"

#include <array>
#include <memory>
#include <optional>

namespace SystemMenu
{

// The arrow that shows how far and in which direction the B button scrolls the Message Board.
struct ScrollArrow
{
    // Where the arrow is, in layout units of the picture.
    Formats::Vector2 position = {0, 0};

    // How long the arrow is. The cursor layout draws it between MinimumArrowLength and MaximumArrowLength.
    float length = 0;

    bool pointsDown = false;

    // False while only the base of the arrow shows, because the pointer has not moved away from where the scrolling began.
    bool isArrowShown = false;
};

// The hands that show where the four remotes point, and the scroll arrow (the layouts of cursor.ash).
class CursorManager
{
public:
    static constexpr float MinimumArrowLength = 32;
    static constexpr float MaximumArrowLength = 128;

    // A hand follows the pointer up to this far beyond the edge of the picture.
    static constexpr float HandMarginBeyondPicture = 100;

    // The display and the files must outlive the cursor manager.
    CursorManager(Display& display, const MenuFiles& files);

    // The hands and the arrow are drawn only while they are visible. The Health and Safety screen switches them off.
    void SetVisible(bool shouldBeVisible);

    // A hand that is hidden is not drawn, for example while the scroll arrow takes its place.
    void SetHandHidden(int playerIndex, bool shouldBeHidden);

    // The hand that has closed around something, as it is while a channel is held, instead of the hand that points.
    void SetHandGrabbing(int playerIndex, bool shouldGrab);

    void ShowScrollArrow(const ScrollArrow& arrow);
    void HideScrollArrow();

    // Once in every step: puts the hands where the remotes point and moves the arrow.
    void Update(const InputManager& input);

    void Draw();

private:
    struct Hand
    {
        bool isHidden = false;
        bool isGrabbing = false;

        // The layout that is drawn for the hand in this step. Nothing while the remote does not point at the picture.
        MenuLayout* shownLayout = nullptr;
    };

    void UpdateHand(int playerIndex, const std::optional<PlayerInput>& playerInput);
    void PlaceHand(MenuLayout& handLayout, const Input::RemoteState& state);
    void UpdateScrollArrow();
    Formats::Vector2 KeepNearPicture(Formats::Vector2 position) const;

    Display& display;
    std::unique_ptr<Layout::LayoutArchive> archive;
    std::array<std::unique_ptr<MenuLayout>, PlayerCount> pointingHands;
    std::array<std::unique_ptr<MenuLayout>, PlayerCount> grabbingHands;
    std::unique_ptr<MenuLayout> scrollArrowLayout;
    std::array<Hand, PlayerCount> hands;
    std::optional<ScrollArrow> scrollArrow;
    bool isVisible = true;
};

}
