#pragma once

#include "SystemMenu/MenuLayout.h"

#include "Formats/Vectors.h"

#include <memory>
#include <string_view>

namespace SystemMenu
{

// The balloon that names a channel or a button while a pointer rests on it (the layout my_IplTopBalloon_a).
// The pointer has to stay for a moment before the balloon shows up. The balloon grows into view and shrinks away again when the pointer leaves.
class Balloon
{
public:
    // The archive holds the layout my_IplTopBalloon_a. The display and the archive must outlive the balloon.
    Balloon(Display& display, const Layout::LayoutArchive& archive);

    // The words in the balloon. The balloon is as wide as its words, and not narrower than the least width that the layout names.
    void SetText(std::u16string_view text);

    // The balloon hangs below the thing it names: the middle of the thing and half of its height. It stays inside the picture.
    void PlaceBelow(Formats::Vector2 targetMiddle, float targetHalfHeight);

    // A pointer is on the thing, or no pointer is on it any more.
    void PointerArrived();
    void PointerLeft();

    // Hides the balloon at once.
    void Hide();

    bool IsShown() const;

    void Calculate();
    void Draw();

private:
    enum class State
    {
        Hidden,
        WaitingToAppear,
        Appearing,
        Shown,
        Disappearing,
    };

    void BecomeHidden();
    void ShowFrameOfTheAnimation();

    Display& display;
    std::unique_ptr<MenuLayout> layout;
    std::size_t animationSlot = 0;
    State state = State::Hidden;
    bool isPointerOnTheTarget = false;
    int stepsWaited = 0;
    float animationFrame = 0;
    float balloonWidth = 0;
};

}
