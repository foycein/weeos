#pragma once

#include "Layout/LiveLayout.h"

#include "Formats/Layout.h"
#include "Graphics/GraphicsDevice.h"

#include <array>

namespace Layout
{

// What the graphics device needs to draw a quad with a material of the layout.
struct DrawableMaterial
{
    Graphics::Material material;

    // The raster colors of the corners, in the order of the quad: left top, right top, right bottom, left bottom.
    std::array<Formats::Rgba8Color, Graphics::QuadCornerCount> cornerColors;
};

// The picture that a texture map of the material shows. It is sent to the device when it is not there yet.
LoadedTexture ObtainTextureOfMap(LiveLayout& layout, const LiveMaterial& liveMaterial, std::size_t textureMapIndex);

// Makes the material for one pane. The picture of every texture is sent to the device when it is not there yet.
//
// The vertex colors of the pane are in the order of the layout file: left top, right top, left bottom, right bottom.
// The alpha of the pane scales the alpha of the colors. The raster color of each corner is made of the vertex color
// and the color of the material, as the channel control of the material says.
//
// A material without stages of its own gets the default stages. The last of them multiplies by the raster color when
// the pane is not fully opaque or the vertex colors are not all white (when the material reads them),
// or when the color of the material, after the alpha of the pane has been applied, is not opaque white (when the material reads it).
DrawableMaterial BuildDrawableMaterial(LiveLayout& layout, const LiveMaterial& liveMaterial, std::uint8_t paneAlpha,
    const std::array<Formats::Rgba8Color, Formats::CornerCount>& vertexColors);

}
