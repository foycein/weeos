#pragma once

#include "SystemMenu/MenuLayout.h"
#include "SystemMenu/WiiMenu/FramePlayer.h"

#include "Formats/Vectors.h"

#include <memory>
#include <string>
#include <string_view>

namespace SystemMenu
{

// The balloon that names a channel or a button of the bar while a pointer rests on it (the layout my_IplTopBalloon_a).
// The pointer has to stay for a while before the balloon grows into view; it shrinks away again when the pointer leaves.
class Balloon
{
public:
    // The archive holds the layout my_IplTopBalloon_a. The display and the archive must outlive the balloon.
    // The pointer has to rest on the thing for the given number of steps before the balloon appears.
    Balloon(Display& display, const Layout::LayoutArchive& archive, int stepsBeforeAppearing);

    // The words in the balloon, shortened with "..." when they are too wide. The balloon is as wide as its words and some space,
    // and not narrower than the least width.
    void SetText(std::u16string_view text);

    // The balloon hangs below the thing it names: the middle of the thing and half of its height. It stays inside the picture.
    void PlaceBelow(Formats::Vector2 targetMiddle, float targetHalfHeight);

    // The middle of the balloon lies at the place, but not closer to the left or right edge of the picture than the margin.
    void PlaceAt(Formats::Vector2 middle, float marginToTheEdge);

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
    int stepsBeforeAppearing = 0;
    int stepsWaited = 0;
    float animationFrame = 0;
    float balloonWidth = 0;
    std::u16string text;
};

}
