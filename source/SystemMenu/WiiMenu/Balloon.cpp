#include "SystemMenu/WiiMenu/Balloon.h"

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

// The balloon is the width of its words and this much more, and at least this wide.
constexpr float SpaceAroundTheText = 40;
constexpr float LeastWidth = 160;

// The balloon hangs this much below the thing it names, less than its own half height and the half height of the thing.
constexpr float OverlapWithTheTarget = 2;

// The balloon keeps this far away from the edges of the picture.
constexpr float MarginToThePictureEdge = 60;

}

Balloon::Balloon(Display& menuDisplay, const Layout::LayoutArchive& archive)
    : display(menuDisplay)
    , layout(std::make_unique<MenuLayout>(menuDisplay, archive, std::string(LayoutName)))
{
    animationSlot = layout->BindAnimationToWholeLayout(std::string(AnimationName), Layout::PlaybackMode::PlayOnce);
    layout->PaneNamed(BalloonPane).isVisible = false;
    ShowFrameOfTheAnimation();
}

void Balloon::SetText(std::u16string_view text)
{
    layout->SetText(TextPane, text);

    float textWidth = layout->MeasureText(TextPane).x;
    balloonWidth = std::max(textWidth + SpaceAroundTheText, LeastWidth);
    layout->PaneNamed(BasePane).width = balloonWidth;
    layout->PaneNamed(ShadePane).width = balloonWidth;
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

    layout->MoveTo({x, y});
}

void Balloon::PointerArrived()
{
    isPointerOnTheTarget = true;
}

void Balloon::PointerLeft()
{
    isPointerOnTheTarget = false;
}

void Balloon::Hide()
{
    isPointerOnTheTarget = false;
    BecomeHidden();
}

void Balloon::BecomeHidden()
{
    state = State::Hidden;
    layout->PaneNamed(BalloonPane).isVisible = false;
}

bool Balloon::IsShown() const
{
    return state != State::Hidden && state != State::WaitingToAppear;
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
            BecomeHidden();
        else if (stepsWaited >= StepsBeforeTheBalloonAppears)
        {
            state = State::Appearing;
            animationFrame = 0;
            layout->PaneNamed(BalloonPane).isVisible = true;
            ShowFrameOfTheAnimation();
        }
        break;

    case State::Appearing:
        animationFrame += 1;
        if (animationFrame >= LastFrameOfTheAnimation)
        {
            animationFrame = LastFrameOfTheAnimation;
            state = State::Shown;
        }
        ShowFrameOfTheAnimation();
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
    if (state == State::Hidden || state == State::WaitingToAppear)
        return;

    layout->Draw();
}

void Balloon::ShowFrameOfTheAnimation()
{
    layout->ShowAnimationFrame(animationSlot, animationFrame);
}

}
