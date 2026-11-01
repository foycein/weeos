#include "SharpArt/SharpPictures.h"

#include "SharpArt/ClockPictures.h"
#include "SharpArt/WiiLogoPictures.h"
#include "SharpArt/WiiMenuPictures.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace SharpArt
{

namespace
{

using enum PictureChannels;

// The clock shows its 48 by 48 digits on panes of 88 by 64 units.
constexpr float ClockMagnification = 2;
constexpr float ShownAtItsSize = 1;
constexpr Color TransparentWhite = {255, 255, 255, 0};
constexpr Color TransparentArrowBlue = {0, 34, 136, 0};
constexpr Color TransparentButtonGray = {193, 193, 193, 0};

constexpr std::array AllPictures = {
    SharpPicture{"my_Clock_a0.tpl", 48, 48, ClockMagnification, Intensity, DrawClockDigitZero},
    SharpPicture{"my_Clock_a1.tpl", 48, 48, ClockMagnification, Intensity, DrawClockDigitOne},
    SharpPicture{"my_Clock_a2.tpl", 48, 48, ClockMagnification, Intensity, DrawClockDigitTwo},
    SharpPicture{"my_Clock_a3.tpl", 48, 48, ClockMagnification, Intensity, DrawClockDigitThree},
    SharpPicture{"my_Clock_a4.tpl", 48, 48, ClockMagnification, Intensity, DrawClockDigitFour},
    SharpPicture{"my_Clock_a5.tpl", 48, 48, ClockMagnification, Intensity, DrawClockDigitFive},
    SharpPicture{"my_Clock_a6.tpl", 48, 48, ClockMagnification, Intensity, DrawClockDigitSix},
    SharpPicture{"my_Clock_a7.tpl", 48, 48, ClockMagnification, Intensity, DrawClockDigitSeven},
    SharpPicture{"my_Clock_a8.tpl", 48, 48, ClockMagnification, Intensity, DrawClockDigitEight},
    SharpPicture{"my_Clock_a9.tpl", 48, 48, ClockMagnification, Intensity, DrawClockDigitNine},
    SharpPicture{"my_Clock_ab.tpl", 48, 48, ClockMagnification, Intensity, DrawClockColon},
    SharpPicture{"my_Clock_b0.tpl", 48, 32, ClockMagnification, Intensity, DrawClockAm},
    SharpPicture{"my_Clock_b1.tpl", 48, 32, ClockMagnification, Intensity, DrawClockPm},
    SharpPicture{"it_BarWii_a.tpl", 72, 33, ShownAtItsSize, ColorAndAlpha, DrawWiiLogoOfTheSettings, TransparentWhite},
    SharpPicture{"my_TopBtn_a0.tpl", 48, 32, ShownAtItsSize, Intensity, DrawEnvelope},
    SharpPicture{"my_TopBtn_a.tpl", 80, 80, ShownAtItsSize, ColorAndAlpha, DrawRoundButton, TransparentButtonGray},
    SharpPicture{"my_TopBtn_c.tpl", 80, 80, ShownAtItsSize, ColorAndAlpha, DrawWiiButton, TransparentButtonGray},
    SharpPicture{"my_arw_a.tpl", 64, 64, ShownAtItsSize, ColorAndAlpha, DrawArrowRight, TransparentArrowBlue},
    SharpPicture{"my_arw_b.tpl", 64, 64, ShownAtItsSize, ColorAndAlpha, DrawArrowLeft, TransparentArrowBlue},
};

constexpr float FullByte = 255;

void TurnIntoIntensity(Formats::RgbaImage& picture)
{
    for (Formats::Rgba8Pixel& pixel : picture.pixels)
    {
        auto intensity = static_cast<Formats::Byte>(std::lround(pixel.red * (pixel.alpha / FullByte)));
        pixel = {intensity, intensity, intensity, intensity};
    }
}

}

std::span<const SharpPicture> AllSharpPictures()
{
    return AllPictures;
}

const SharpPicture* FindSharpPicture(std::string_view textureFileName)
{
    for (const SharpPicture& picture : AllPictures)
    {
        if (picture.textureFileName == textureFileName)
            return &picture;
    }
    return nullptr;
}

Formats::RgbaImage DrawSharpPicture(const SharpPicture& sharpPicture, std::uint32_t pixelsPerUnit)
{
    VectorCanvas canvas(sharpPicture.width, sharpPicture.height, pixelsPerUnit, sharpPicture.transparentColor);
    sharpPicture.draw(canvas);

    Formats::RgbaImage picture = std::move(canvas.Picture());
    if (sharpPicture.channels == Intensity)
        TurnIntoIntensity(picture);
    return picture;
}

std::uint32_t PixelsPerUnitFor(const SharpPicture& sharpPicture, float screenPixelsPerUnit)
{
    float needed = std::ceil(screenPixelsPerUnit * sharpPicture.largestMagnification);
    return static_cast<std::uint32_t>(std::clamp(needed, 1.0f, static_cast<float>(MostPixelsPerUnit)));
}

}
