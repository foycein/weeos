#pragma once

#include "Graphics/GraphicsDevice.h"

#include <vector>

namespace Graphics
{

// The stages a material is drawn with. These are the stages of the material, or, when it has none, the standard chain
// for its number of textures. The standard chain is what the layout library of the original menu sets up for such a material:
//
//  - no texture:   the raster color, tinted by REG1
//  - one texture:  the texture, with REG0 for black and REG1 for white
//  - two textures: texture 0 mixed with texture 1 by the alpha of constant color 3, then tinted like one texture
//  - three:        the textures added with the weights in constant color 3 (alpha, blue and green), then tinted like one texture
//
// A chain with a texture ends with a stage that multiplies the result with the raster color. For a quad with white
// raster colors that changes nothing. The tint stage after two or three textures is left out while REG0 is black and REG1 is white.
std::vector<TevStage> ChooseStages(const Material& material);

}
