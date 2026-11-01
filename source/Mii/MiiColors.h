#pragma once

#include "Formats/RgbaImage.h"

#include <array>

namespace Mii
{

// The colors of the face library (docs/research/Mii.md section 9.1). A record's color field is the index into its table.
// The hair table also colors the eyebrows and the facial hair.
extern const std::array<Formats::Rgba8Color, 6> SkinColors;
extern const std::array<Formats::Rgba8Color, 8> HairColors;
extern const std::array<Formats::Rgba8Color, 6> GlassesColors;
extern const std::array<Formats::Rgba8Color, 12> FavoriteColors;
extern const std::array<Formats::Rgba8Color, 6> EyeColors;
extern const std::array<Formats::Rgba8Color, 3> LipColors;
extern const std::array<Formats::Rgba8Color, 3> UpperLipColors;
extern const Formats::Rgba8Color MoleColor;

// The outline of the eyes is black, except for two eye pictures that have a colored line in their red channel.
Formats::Rgba8Color EyeOutlineColor(int eyeType);

}
