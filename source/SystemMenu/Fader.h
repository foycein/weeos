#pragma once

#include "SystemMenu/Display.h"

#include <cstdint>

namespace SystemMenu
{

// Where the fader is between the picture and the black that covers it.
enum class FaderStatus
{
    // The black covers the whole picture.
    Covered,

    // The picture shows.
    Clear,

    // Going from Covered to Clear.
    FadingIn,

    // Going from Clear to Covered.
    FadingOut,
};

// Fades the whole picture to black and back. A screen that ends fades out and waits until the status is Covered,
// the screen that follows fades in. Every step the menu calls Calculate, and Draw paints the black in front of everything.
class Fader
{
public:
    // The fader starts Covered, so the picture is black until the first screen has built its layouts.
    explicit Fader(Display& display);

    FaderStatus GetStatus() const;

    void SetCovered();
    void SetClear();

    // Starts the fade from Covered to Clear. Gives back false, and changes nothing, in any other status.
    bool StartFadeIn();

    // Starts the fade from Clear to Covered. Gives back false, and changes nothing, in any other status.
    bool StartFadeOut();

    // One step of the fade, and the darkness of the black for the next Draw.
    void Calculate();

    // Draws nothing while the black is fully transparent.
    void Draw();

private:
    // Counts a step of the fade that runs and gives the number of steps that were done before it.
    std::uint32_t TakeStepOfFade();

    void CalculateFadeIn();
    void CalculateFadeOut();

    Display& display;
    FaderStatus status = FaderStatus::Covered;
    std::uint32_t stepsInFade = 0;
    std::uint8_t blackAlpha = 255;
};

}
