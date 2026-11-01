#include "SharpArt/SharpPictures.h"

#include "SharpArt/ClockPictures.h"
#include "SharpArt/WiiLogoPictures.h"
#include "SharpArt/WiiMenuPictures.h"
#include "SharpArt/WiiOptionsPictures.h"

#include "Formats/Crc32.h"

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
constexpr Color TransparentCardBlue = {51, 187, 238, 0};

constexpr std::array AllPictures = {
    SharpPicture{"my_Clock_a0.tpl", 48, 48, 0xFD378172, ClockMagnification, Intensity, DrawClockDigitZero},
    SharpPicture{"my_Clock_a1.tpl", 48, 48, 0xC4473943, ClockMagnification, Intensity, DrawClockDigitOne},
    SharpPicture{"my_Clock_a2.tpl", 48, 48, 0xEF1E74F5, ClockMagnification, Intensity, DrawClockDigitTwo},
    SharpPicture{"my_Clock_a3.tpl", 48, 48, 0x36F2C3CE, ClockMagnification, Intensity, DrawClockDigitThree},
    SharpPicture{"my_Clock_a4.tpl", 48, 48, 0x5298AB2A, ClockMagnification, Intensity, DrawClockDigitFour},
    SharpPicture{"my_Clock_a5.tpl", 48, 48, 0x7A1CB524, ClockMagnification, Intensity, DrawClockDigitFive},
    SharpPicture{"my_Clock_a6.tpl", 48, 48, 0x3B2946B9, ClockMagnification, Intensity, DrawClockDigitSix},
    SharpPicture{"my_Clock_a7.tpl", 48, 48, 0x3E42D6A7, ClockMagnification, Intensity, DrawClockDigitSeven},
    SharpPicture{"my_Clock_a8.tpl", 48, 48, 0x964794FB, ClockMagnification, Intensity, DrawClockDigitEight},
    SharpPicture{"my_Clock_a9.tpl", 48, 48, 0x7DABE0B4, ClockMagnification, Intensity, DrawClockDigitNine},
    SharpPicture{"my_Clock_ab.tpl", 48, 48, 0x6A0C754B, ClockMagnification, Intensity, DrawClockColon},
    SharpPicture{"my_Clock_b0.tpl", 48, 32, 0xD4773177, ClockMagnification, Intensity, DrawClockAm},
    SharpPicture{"my_Clock_b1.tpl", 48, 32, 0xB538DE0E, ClockMagnification, Intensity, DrawClockPm},
    SharpPicture{"it_Setting_a.tpl", 256, 200, 0x55B890A4, ShownAtItsSize, ColorAndAlpha, DrawWiiSettingsButton, TransparentWhite},
    SharpPicture{"it_BarWii_a.tpl", 72, 33, 0x9988B105, ShownAtItsSize, ColorAndAlpha, DrawWiiLogoOfTheSettings, TransparentWhite},
    SharpPicture{"my_TopBtn_a0.tpl", 48, 32, 0x4B6F0A3E, ShownAtItsSize, Intensity, DrawEnvelope},
    SharpPicture{"my_TopBtn_a.tpl", 80, 80, 0xD054B06E, ShownAtItsSize, ColorAndAlpha, DrawRoundButton, TransparentButtonGray},
    SharpPicture{"my_TopBtn_c.tpl", 80, 80, 0x1ED4FB29, ShownAtItsSize, ColorAndAlpha, DrawWiiButton, TransparentButtonGray},
    SharpPicture{"my_TopBtn_h.tpl", 80, 80, 0x4A42E90D, ShownAtItsSize, Intensity, DrawButtonGlowRing},
    SharpPicture{"mn_Top_SdCard_b.tpl", 38, 48, 0x88BBE31B, ShownAtItsSize, ColorAndAlpha, DrawSdCard, TransparentCardBlue},
    SharpPicture{"my_arw_a.tpl", 64, 64, 0x15ECDE96, ShownAtItsSize, ColorAndAlpha, DrawArrowRight, TransparentArrowBlue},
    SharpPicture{"my_arw_b.tpl", 64, 64, 0x2BF2CD28, ShownAtItsSize, ColorAndAlpha, DrawArrowLeft, TransparentArrowBlue},
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

std::uint32_t FingerprintOf(const Formats::RgbaImage& picture)
{
    Formats::Bytes bytes;
    bytes.reserve(picture.pixels.size() * 4);
    for (const Formats::Rgba8Pixel& pixel : picture.pixels)
    {
        bytes.push_back(pixel.red);
        bytes.push_back(pixel.green);
        bytes.push_back(pixel.blue);
        bytes.push_back(pixel.alpha);
    }
    return Formats::ComputeCrc32(bytes);
}

bool IsTheOriginalOf(const SharpPicture& sharpPicture, const Formats::RgbaImage& picture)
{
    if (picture.width != sharpPicture.width || picture.height != sharpPicture.height)
        return false;

    return FingerprintOf(picture) == sharpPicture.originalFingerprint;
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
