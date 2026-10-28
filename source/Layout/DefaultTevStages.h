#pragma once

#include "Graphics/TevStage.h"

#include <cstddef>
#include <vector>

namespace Layout
{

// A material without stages of its own gets the stages that fit the number of its textures:
//     no texture:      register 1 times the raster color
//     one texture:     the texture, tinted between register 0 (black by default) and register 1 (white by default)
//     two textures:    the first texture, blended with the second one by the alpha of constant color 3
//     three textures:  the textures, each weighted by one channel of constant color 3 and 2, added up
// With two or more textures a last tint between register 0 and register 1 follows, unless the registers are black and white.
// If multipliesByRasterColor is set, a last stage multiplies everything by the raster color (the colors of the corners and the pane alpha).
//
// More than Graphics::MaximumTextureCount textures throw a LayoutError.
std::vector<Graphics::TevStage> MakeDefaultTevStages(std::size_t textureCount, bool tintsTextures, bool multipliesByRasterColor);

}
