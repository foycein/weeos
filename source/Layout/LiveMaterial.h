#pragma once

#include "Formats/LayoutMaterial.h"
#include "Graphics/GraphicsDevice.h"

#include <cstddef>
#include <string>
#include <vector>

namespace Layout
{

// A material while the layout is alive.
struct LiveMaterial
{
    // The material as the file describes it. The animations change its color registers, its constant colors,
    // its material color and its texture transforms.
    Formats::Material description;

    // The texture file shown by every texture map of the material, for example "my_Balloon_a.tpl".
    // The texture pattern animations change them.
    std::vector<std::string> textureNames;

    // The stages of the file, ready for the graphics device. There are none when the material uses the default stages.
    std::vector<Graphics::TevStage> stages;

    Graphics::BlendMode blendMode = Graphics::BlendMode::StandardAlpha;
};

// Checks that the library can draw the material and prepares it. The texture names are the list of the layout.
// Throws a LayoutError when the material uses something that is not supported.
LiveMaterial MakeLiveMaterial(Formats::Material description, const std::vector<std::string>& layoutTextureNames);

// The texture coordinate generators of a material say for every texture map which set of texture coordinates it reads
// and which texture transform moves them.
constexpr std::size_t TextureCoordinateSourceBase = 4;

// The number of the set of texture coordinates (0 to 2) that the picture or window has to supply for the texture map.
std::size_t TextureCoordinateSetOf(const Formats::Material& material, std::size_t textureMapIndex);

// The matrix that moves the texture coordinates of the texture map: the texture transform that its generator names,
// or no change at all when the generator names none.
Graphics::TextureMatrix TextureMatrixOf(const Formats::Material& material, std::size_t textureMapIndex);

}
