#include "SystemMenu/Fader.h"

#include <algorithm>

namespace SystemMenu
{

namespace
{

// A fade changes the black by a twentieth of the full black in each step.
constexpr std::uint32_t FadeStepCount = 20;

// A fade out holds the full black for one more step before the status becomes Covered.
constexpr std::uint32_t StepsOfFullBlackBeforeCovered = 1;

constexpr std::uint8_t NoBlack = 0;
constexpr std::uint8_t FullBlack = 255;

constexpr Formats::Rgba8Color Black = {0, 0, 0, 0};

// How much black a fade out has made after this many steps. A fade in is the same the other way round.
std::uint8_t BlackAfterSteps(std::uint32_t steps)
{
    return static_cast<std::uint8_t>(steps * FullBlack / FadeStepCount);
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
    switch (status)
    {
    case FaderStatus::Clear:
        blackAlpha = NoBlack;
        return;

    case FaderStatus::Covered:
        blackAlpha = FullBlack;
        return;

    case FaderStatus::FadingIn:
        CalculateFadeIn();
        return;

    case FaderStatus::FadingOut:
        CalculateFadeOut();
        return;
    }
}

std::uint32_t Fader::TakeStepOfFade()
{
    std::uint32_t stepsDone = stepsInFade;
    ++stepsInFade;
    return stepsDone;
}

void Fader::CalculateFadeIn()
{
    std::uint32_t stepsDone = TakeStepOfFade();
    if (stepsDone > FadeStepCount)
        status = FaderStatus::Clear;

    blackAlpha = FullBlack - BlackAfterSteps(std::min(stepsDone, FadeStepCount));
}

void Fader::CalculateFadeOut()
{
    std::uint32_t stepsDone = TakeStepOfFade();
    if (stepsDone > FadeStepCount + StepsOfFullBlackBeforeCovered)
        status = FaderStatus::Covered;

    blackAlpha = BlackAfterSteps(std::min(stepsDone, FadeStepCount));
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
