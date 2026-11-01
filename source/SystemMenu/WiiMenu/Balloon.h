#pragma once

#include "SystemMenu/MenuLayout.h"
#include "SystemMenu/WiiMenu/FramePlayer.h"

#include "Formats/Vectors.h"

#include <memory>
#include <string>
#include <string_view>

namespace SystemMenu
{

// The balloon that names a channel while a pointer rests on it (the layout my_IplTopBalloon_a). Every channel of the grid has one.
// The pointer has to stay for 20 steps before the balloon grows into view; it shrinks away again when the pointer leaves.
class Balloon
{
public:
    // The archive holds the layout my_IplTopBalloon_a. The display and the archive must outlive the balloon.
    Balloon(Display& display, const Layout::LayoutArchive& archive);

    // The words in the balloon, shortened with "..." when they are too wide. The balloon is as wide as its words and some space,
    // and not narrower than the least width.
    void SetText(std::u16string_view text);

    // The balloon hangs below the thing it names: the middle of the thing and half of its height. It stays inside the picture.
    void PlaceBelow(Formats::Vector2 targetMiddle, float targetHalfHeight);

    // A pointer is on the thing, or no pointer is on it any more.
    void PointerArrived();
    void PointerLeft();

    bool IsShown() const;
    const std::u16string& Text() const;

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
    void ShortenUntilItFits();
    float TextWidth();

    Display& display;
    std::unique_ptr<MenuLayout> layout;
    std::unique_ptr<FramePlayer> growing;
    State state = State::Hidden;
    bool isPointerOnTheTarget = false;
    int stepsWaited = 0;
    float animationFrame = 0;
    float balloonWidth = 0;
    std::u16string text;
};

}
