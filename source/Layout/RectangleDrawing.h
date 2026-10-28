#pragma once

#include "Layout/LiveLayout.h"
#include "Layout/PanePlacement.h"
#include "Layout/PaneRectangle.h"

#include "Formats/Layout.h"
#include "Graphics/GraphicsDevice.h"

#include <array>
#include <cstdint>
#include <span>

namespace Layout
{

// What a rectangle of a pane looks like: its material, its vertex colors and its sets of texture coordinates.
// The vertex colors and the corners of the texture coordinates are in the order of the layout file:
// left top, right top, left bottom, right bottom.
struct RectangleAppearance
{
    std::uint16_t materialIndex = 0;
    std::array<Formats::Rgba8Color, Formats::CornerCount> vertexColors;
    std::span<const Formats::TextureCoordinateSet> textureCoordinateSets;
};

// Draws one quad for the rectangle of the pane, placed as the placement says.
// Throws a LayoutError when the material reads a set of texture coordinates that the appearance does not have.
void DrawRectangle(Graphics::GraphicsDevice& device, LiveLayout& layout, const PanePlacement& placement,
    const PaneRectangle& rectangle, const RectangleAppearance& appearance);

}
