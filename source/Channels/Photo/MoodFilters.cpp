#include "Channels/Photo/MoodFilters.h"

#include <algorithm>
#include <cmath>

namespace Channels
{

namespace
{

constexpr float White = 255;
constexpr float Middle = 128;

// Brighten moves every color towards white by this share per step, so the eighth step is two thirds of the way.
constexpr float BrightenSharePerStep = 1.0f / 12;

// Hard-boiled is a gray picture whose contrast grows by this much per step.
constexpr float HardBoiledContrastPerStep = 0.35f;

// Zap! is the drawing of a shocked face in a comic: the dark half becomes deep blue, the light half pale blue.
constexpr Formats::Rgba8Pixel ZapDark = {20, 24, 96, 255};
constexpr Formats::Rgba8Pixel ZapLight = {210, 228, 255, 255};

constexpr float RedWeight = 0.299f;
constexpr float GreenWeight = 0.587f;
constexpr float BlueWeight = 0.114f;

Formats::Byte ToByte(float value)
{
    return static_cast<Formats::Byte>(std::lround(std::clamp(value, 0.0f, White)));
}

template <typename ChangePixel>
Formats::RgbaImage ChangeEveryPixel(const Formats::RgbaImage& photo, ChangePixel changePixel)
{
    Formats::RgbaImage changed = photo;
    for (Formats::Rgba8Pixel& pixel : changed.pixels)
        pixel = changePixel(pixel);

    return changed;
}

int LimitedStep(int step)
{
    return std::clamp(step, 1, StrongestMoodStep);
}

}

float BrightnessOf(const Formats::Rgba8Pixel& pixel)
{
    return RedWeight * pixel.red + GreenWeight * pixel.green + BlueWeight * pixel.blue;
}

Formats::RgbaImage Brighten(const Formats::RgbaImage& photo, int step)
{
    float share = BrightenSharePerStep * LimitedStep(step);
    return ChangeEveryPixel(photo, [share](const Formats::Rgba8Pixel& pixel) {
        auto brighter = [share](Formats::Byte value) { return ToByte(value + (White - value) * share); };
        return Formats::Rgba8Pixel{brighter(pixel.red), brighter(pixel.green), brighter(pixel.blue), pixel.alpha};
    });
}

Formats::RgbaImage BlackAndWhite(const Formats::RgbaImage& photo)
{
    return ChangeEveryPixel(photo, [](const Formats::Rgba8Pixel& pixel) {
        Formats::Byte gray = ToByte(BrightnessOf(pixel));
        return Formats::Rgba8Pixel{gray, gray, gray, pixel.alpha};
    });
}

Formats::RgbaImage Zap(const Formats::RgbaImage& photo)
{
    return ChangeEveryPixel(photo, [](const Formats::Rgba8Pixel& pixel) {
        Formats::Rgba8Pixel zapped = BrightnessOf(pixel) < Middle ? ZapDark : ZapLight;
        zapped.alpha = pixel.alpha;
        return zapped;
    });
}

Formats::RgbaImage HardBoiled(const Formats::RgbaImage& photo, int step)
{
    float contrast = 1 + HardBoiledContrastPerStep * LimitedStep(step);
    return ChangeEveryPixel(photo, [contrast](const Formats::Rgba8Pixel& pixel) {
        Formats::Byte gray = ToByte((BrightnessOf(pixel) - Middle) * contrast + Middle);
        return Formats::Rgba8Pixel{gray, gray, gray, pixel.alpha};
    });
}

}
