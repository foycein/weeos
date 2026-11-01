#pragma once

#include "Formats/RgbaImage.h"

namespace Channels
{

// The moods of Fun! (layout "filter"). Brighten and Hard-boiled get stronger each time they are chosen again, up to the eighth step.
// The original does them with the graphics hardware; the formulas here are WeeOS's own (Assumed, docs/audit/OpenPoints/Channels.md CHN-33).
constexpr int StrongestMoodStep = 8;

// The brightness of a color as television measures it (ITU-R BT.601), from 0 to 255.
float BrightnessOf(const Formats::Rgba8Pixel& pixel);

Formats::RgbaImage Brighten(const Formats::RgbaImage& photo, int step);
Formats::RgbaImage BlackAndWhite(const Formats::RgbaImage& photo);
Formats::RgbaImage Zap(const Formats::RgbaImage& photo);
Formats::RgbaImage HardBoiled(const Formats::RgbaImage& photo, int step);

}
