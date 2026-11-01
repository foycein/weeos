#include "SystemMenu/WiiMenu/Balloon.h"

#include "SystemMenu/WiiMenu/LayoutParts.h"

#include <algorithm>

namespace SystemMenu
{

namespace
{

constexpr std::string_view LayoutName = "my_IplTopBalloon_a";
constexpr std::string_view AnimationName = "my_IplTopBalloon_a_BalloonInOut";

constexpr std::string_view TextPane = "T_Balloon";
constexpr std::string_view BasePane = "W_Base";
constexpr std::string_view ShadePane = "W_Shade";
constexpr std::string_view BalloonPane = "N_Balloon";

// The pointer rests on the thing for this many steps before the balloon appears.
constexpr int StepsBeforeTheBalloonAppears = 20;

// The animation has 7 frames. The balloon shows the first one when it is gone and the last one when it is there.
constexpr float LastFrameOfTheAnimation = 6;

// The words may be this wide. Wider words lose characters at their end and get "..." instead.
constexpr float WidestText = 391.5f;
constexpr std::u16string_view Ellipsis = u"...";

// The balloon is the width of its words and this much more, and at least this wide.
constexpr float SpaceAroundTheText = 40;
constexpr float LeastWidth = 160;

// Assumed: the balloon hangs this much into the thing it names, and keeps this far away from the edges of the picture.
constexpr float OverlapWithTheTarget = 2;
constexpr float MarginToThePictureEdge = 60;

}

Balloon::Balloon(Display& menuDisplay, const Layout::LayoutArchive& archive)
    : display(menuDisplay)
    , layout(std::make_unique<MenuLayout>(menuDisplay, archive, std::string(LayoutName)))
{
    animationSlot = layout->BindAnimationToAllPanes(std::string(AnimationName));
    BecomeHidden();
}

void Balloon::SetText(std::u16string_view newText)
{
    text = newText;
    layout->SetText(TextPane, text);
    ShortenUntilItFits();

    float textWidth = MeasureTextWidth(*layout, display, TextPane);
    balloonWidth = std::max(textWidth + SpaceAroundTheText, LeastWidth);
    layout->PaneNamed(BasePane).width = balloonWidth;
    layout->PaneNamed(ShadePane).width = balloonWidth;
}

void Balloon::ShortenUntilItFits()
{
    std::u16string shortened = text;
    while (!shortened.empty() && MeasureTextWidth(*layout, display, TextPane) > WidestText)
    {
        shortened.pop_back();
        layout->SetText(TextPane, shortened + std::u16string(Ellipsis));
    }
}

void Balloon::PlaceBelow(Formats::Vector2 targetMiddle, float targetHalfHeight)
{
    float halfHeight = layout->PaneNamed(BasePane).height / 2;
    float y = targetMiddle.y - (targetHalfHeight + halfHeight - OverlapWithTheTarget);

    float pictureHalfWidth = display.GetCanvas().width / 2;
    float spaceAtTheLeft = (targetMiddle.x - balloonWidth / 2) + pictureHalfWidth;
    float spaceAtTheRight = pictureHalfWidth - (targetMiddle.x + balloonWidth / 2);

    float x = targetMiddle.x;
    if (spaceAtTheLeft < MarginToThePictureEdge)
        x += MarginToThePictureEdge - spaceAtTheLeft;
    else if (spaceAtTheRight < MarginToThePictureEdge)
        x -= MarginToThePictureEdge - spaceAtTheRight;

    MoveLayoutTo(*layout, {x, y});
}

void Balloon::PointerArrived()
{
    isPointerOnTheTarget = true;
}

void Balloon::PointerLeft()
{
    isPointerOnTheTarget = false;
}

bool Balloon::IsShown() const
{
    return state != State::Hidden && state != State::WaitingToAppear;
}

const std::u16string& Balloon::Text() const
{
    return text;
}

void Balloon::BecomeHidden()
{
    state = State::Hidden;
    animationFrame = 0;
    layout->PaneNamed(BalloonPane).isVisible = false;
    ShowFrameOfTheAnimation();
}

void Balloon::Calculate()
{
    switch (state)
    {
    case State::Hidden:
        if (isPointerOnTheTarget)
        {
            state = State::WaitingToAppear;
            stepsWaited = 0;
        }
        break;

    case State::WaitingToAppear:
        ++stepsWaited;
        if (!isPointerOnTheTarget)
        {
            BecomeHidden();
        }
        else if (stepsWaited >= StepsBeforeTheBalloonAppears)
        {
            state = State::Appearing;
            layout->PaneNamed(BalloonPane).isVisible = true;
        }
        break;

    case State::Appearing:
        animationFrame = std::min(animationFrame + 1, LastFrameOfTheAnimation);
        ShowFrameOfTheAnimation();
        if (animationFrame == LastFrameOfTheAnimation)
            state = State::Shown;
        break;

    case State::Shown:
        if (!isPointerOnTheTarget)
            state = State::Disappearing;
        break;

    case State::Disappearing:
        animationFrame -= 1;
        ShowFrameOfTheAnimation();
        if (animationFrame <= 0)
            BecomeHidden();
        break;
    }

    layout->Calculate();
}

void Balloon::Draw()
{
    if (!IsShown())
        return;

    layout->Draw();
}

void Balloon::ShowFrameOfTheAnimation()
{
    layout->ShowAnimationFrame(animationSlot, animationFrame);
}

}
