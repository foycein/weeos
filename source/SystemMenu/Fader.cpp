#include "SystemMenu/Fader.h"

namespace SystemMenu
{

namespace
{

// A fade takes this many steps from the first step to the last one at which the black is not all there or all gone.
constexpr std::uint32_t FadeStepCount = 20;

// A fade out holds the full black for one more step before the status becomes Covered.
constexpr std::uint32_t StepsOfFullBlackBeforeCovered = 1;

constexpr std::uint8_t NoBlack = 0;
constexpr std::uint8_t FullBlack = 255;

constexpr Formats::Rgba8Color Black = {0, 0, 0, 0};

std::uint8_t BlackAfterStepsOfFadeOut(std::uint32_t stepsDone)
{
    return static_cast<std::uint8_t>(stepsDone * FullBlack / FadeStepCount);
}

}

Fader::Fader(Display& displayToCover)
    : display(displayToCover)
{
}

FaderStatus Fader::GetStatus() const
{
    return status;
}

void Fader::SetCovered()
{
    status = FaderStatus::Covered;
    blackAlpha = FullBlack;
}

void Fader::SetClear()
{
    status = FaderStatus::Clear;
    blackAlpha = NoBlack;
}

bool Fader::StartFadeIn()
{
    if (status != FaderStatus::Covered)
        return false;

    status = FaderStatus::FadingIn;
    stepsInFade = 0;
    return true;
}

bool Fader::StartFadeOut()
{
    if (status != FaderStatus::Clear)
        return false;

    status = FaderStatus::FadingOut;
    stepsInFade = 0;
    return true;
}

void Fader::Calculate()
{
    if (status == FaderStatus::Clear)
    {
        blackAlpha = NoBlack;
        return;
    }

    if (status == FaderStatus::Covered)
    {
        blackAlpha = FullBlack;
        return;
    }

    std::uint32_t stepsDone = stepsInFade;
    ++stepsInFade;

    if (status == FaderStatus::FadingIn)
    {
        if (stepsDone > FadeStepCount)
        {
            status = FaderStatus::Clear;
            stepsDone = FadeStepCount;
        }
        blackAlpha = FullBlack - BlackAfterStepsOfFadeOut(stepsDone);
        return;
    }

    if (stepsDone > FadeStepCount)
    {
        if (stepsDone > FadeStepCount + StepsOfFullBlackBeforeCovered)
            status = FaderStatus::Covered;

        stepsDone = FadeStepCount;
    }
    blackAlpha = BlackAfterStepsOfFadeOut(stepsDone);
}

void Fader::Draw()
{
    if (blackAlpha == NoBlack)
        return;

    Graphics::Canvas canvas = display.GetCanvas();
    float halfWidth = canvas.width / 2;
    float halfHeight = canvas.height / 2;
    Formats::Rgba8Color color = Black;
    color.alpha = blackAlpha;

    Graphics::Quad quad = {};
    quad.positions = {{{-halfWidth, halfHeight}, {halfWidth, halfHeight}, {halfWidth, -halfHeight}, {-halfWidth, -halfHeight}}};
    quad.colors = {{color, color, color, color}};
    display.Device().DrawQuad(quad, Graphics::Material());
}

}
